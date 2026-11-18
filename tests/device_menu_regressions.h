#pragma once
#include "device_matrix_regressions.h"
#include "device_custom_info_regressions.h"
#include "firmware_menu_patch.h"
#include <array>
#include <cstring>
#include <cstdlib>

namespace DeviceMenuRegressionDetail {
namespace CurrentExecutor {
#include "firmware_menu_executor.inc"
}
using DeviceFunctionalMatrixDetail::Bench;
using DeviceFunctionalMatrixDetail::Power;
using FirmwareDevice::ObserveRegressionState;
inline uint32_t Word(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void Word(std::vector<uint8_t>& p, size_t at, uint32_t v) {
    for (unsigned i=0;i<4;++i) p[at+i]=uint8_t(v>>(i*8));
}
inline bool BranchTarget(const uint8_t* bytes, uint32_t pc, uint32_t& target, bool& link) {
    const uint32_t instruction=Word(bytes), a=instruction&0xffff, b=instruction>>16;
    if ((a&0xf800)!=0xf000 || ((b&0xd000)!=0x9000 && (b&0xd000)!=0xd000)) return false;
    const uint32_t s=(a>>10)&1;
    const uint32_t bits=s<<24|((~((b>>13&1)^s))&1)<<23|((~((b>>11&1)^s))&1)<<22|
        (a&0x3ff)<<12|(b&0x7ff)<<1;
    target=pc+4+(int32_t(bits<<7)>>7);
    link=(b&0x4000)!=0;
    return true;
}
inline bool Variants(const std::vector<uint8_t>& inner, const FirmwarePackage& stock,
    const FirmwarePackage& all, std::string& error) {
    for (uint32_t mask=1;mask<=FirmwareMenu::DefaultMask;++mask) {
        std::array<FirmwareMenu::Item,3> order{FirmwareMenu::Item::Information,
            FirmwareMenu::Item::Voltage,FirmwareMenu::Item::Exit};
        do {
            for (bool reverse:{false,true}) {
                FirmwareMenu::Settings settings{true,true,reverse};
                settings.enabledItems=mask;
                unsigned index=0;
                for (const auto item:order) {
                    if (item==FirmwareMenu::Item::Exit) {
                        settings.order[index++]=FirmwareMenu::Item::PuffEditor;
                        settings.order[index++]=FirmwareMenu::Item::ScoreEditor;
                        settings.order[index++]=FirmwareMenu::Item::RankSelector;
                    }
                    settings.order[index++]=item;
                }
                auto bytes=inner;
                BuildReport report;
                FirmwarePackage package;
                if (!FirmwareMenu::Apply(bytes,settings,report,error) ||
                    !FirmwarePackage::Parse(bytes,package,error)) return false;
                for (size_t i=0;i<package.records.size();++i) {
                    const auto& patched=package.records[i];
                    const auto& original=stock.records[i];
                    const auto costs=FirmwareMenu::FlashCosts(original,settings);
                    if (costs.total!=patched.image.size()-original.image.size() ||
                        costs.common+costs.information+costs.voltage!=costs.total ||
                        !costs.common || ((mask&FirmwareMenu::InformationMask)!=0)!=(costs.information!=0) ||
                        ((mask&FirmwareMenu::VoltageMask)!=0)!=(costs.voltage!=0)) {
                        error="menu cost attribution double-counted or omitted native bytes"; return false;
                    }
                    if (patched.image.size()-original.image.size()!=FirmwareMenu::FlashBytes(original,settings) ||
                        (mask!=FirmwareMenu::DefaultMask && patched.image.size()>=all.records[i].image.size())) {
                        error="disabled native module remained in Flash or exact size prediction disagreed"; return false;
                    }
                    if (mask==FirmwareMenu::InformationMask) {
                        const auto& full=all.records[i];
                        const size_t footer=full.image.size()-24, start=Word(full.image.data()+footer+8);
                        const uint32_t voltage=full.linked_base+uint32_t(start+64+CurrentExecutor::All::VoltageOffset);
                        unsigned found=0;
                        for (size_t at=0;at+4<=original.image.size();at+=2) {
                            if (std::memcmp(full.image.data()+at,original.image.data()+at,4)==0) continue;
                            uint32_t target=0; bool link=false;
                            if (BranchTarget(full.image.data()+at,full.linked_base+uint32_t(at),target,link) && target==voltage) {
                                if (std::memcmp(patched.image.data()+at,original.image.data()+at,4)!=0) {
                                    error="disabled voltage module still patched the native voltage renderer"; return false;
                                }
                                ++found;
                            }
                        }
                        if (found!=1) { error="native voltage hook omission was not checked exactly once"; return false; }
                    }
                }
                FirmwareMenu::Settings restored;
                if (!FirmwareMenu::Extract(package,restored,error) || restored!=settings) {
                    if (error.empty()) error="native item order, modules or direction did not roundtrip"; return false;
                }
                for (size_t i=0;i<package.records.size();++i) if (package.records[i].image!=stock.records[i].image) {
                    error="modular native menu failed to restore exact inner firmware"; return false;
                }
            }
        } while (std::next_permutation(order.begin(),order.end()));
    }
    for (unsigned kind=0;kind<6;++kind) {
        FirmwareMenu::Settings invalid{true,false};
        if (kind==0) invalid.enabledItems=0;
        else if (kind==1) invalid.enabledItems=32;
        else if (kind==2) invalid.order[1]=invalid.order[0];
        else if (kind==3) invalid.order[2]=static_cast<FirmwareMenu::Item>(6);
        else invalid.infoFields=kind==4?8:0;
        auto bytes=inner;
        BuildReport report;
        std::string rejected;
        if (FirmwareMenu::ValidSettings(invalid) || FirmwareMenu::Apply(bytes,invalid,report,rejected) || bytes!=inner) {
            error="invalid modular settings were accepted or changed rejected firmware output"; return false;
        }
    }
    return true;
}
inline bool FieldVariants(const std::vector<uint8_t>& inner, const FirmwarePackage& stock, std::string& error) {
    for (uint32_t modules:{FirmwareMenu::InformationMask,FirmwareMenu::DefaultMask})
        for (uint32_t fields=1;fields<=FirmwareMenu::AllInfoFields;++fields) {
            FirmwareMenu::Settings settings{true,false};
            settings.enabledItems=modules;
            settings.infoFields=fields;
            auto bytes=inner;
            BuildReport report;
            FirmwarePackage package;
            if (!FirmwareMenu::Apply(bytes,settings,report,error) || !FirmwarePackage::Parse(bytes,package,error)) return false;
            for (size_t i=0;i<package.records.size();++i) {
                const auto& original=stock.records[i];
                auto full=settings;
                full.infoFields=FirmwareMenu::AllInfoFields;
                const auto costs=FirmwareMenu::FlashCosts(original,settings);
                const size_t added=package.records[i].image.size()-original.image.size();
                if (added!=costs.total || costs.common+costs.information+costs.voltage!=added ||
                    (fields!=FirmwareMenu::AllInfoFields && added>=FirmwareMenu::FlashBytes(original,full))) {
                    error="an omitted INFO field remained in firmware or exact byte attribution disagreed"; return false;
                }
                for (size_t padding=0;padding<4;++padding) {
                    const auto aligned=FirmwareMenu::FlashCosts(original,settings,original.image.size()+padding);
                    if (aligned.total!=FirmwareMenu::FlashBytes(original,settings,original.image.size()+padding) ||
                        aligned.common+aligned.information+aligned.voltage!=aligned.total) {
                        error="native module cost attribution lost alignment bytes"; return false;
                    }
                }
            }
            FirmwareMenu::Settings restored;
            if (!FirmwareMenu::Extract(package,restored,error) || restored!=settings) return false;
            for (size_t i=0;i<package.records.size();++i) if (package.records[i].image!=stock.records[i].image) {
                error="INFO field subset did not restore exact inner firmware"; return false;
            }
        }
    return true;
}
inline bool Capture(Bench& b, const StudioState& source, uint16_t code, const char* phase) {
    size_t size=0;
    if(getenv_s(&size,nullptr,0,"VFS_MENU_CAPTURE")!=0 || !size) return true;
    std::vector<char> prefix(size);
    if(getenv_s(&size,prefix.data(),prefix.size(),"VFS_MENU_CAPTURE")!=0) {
        b.error="Could not read native menu framebuffer capture prefix"; return false;
    }
    std::ostringstream path;
    path<<prefix.data()<<'-'<<(source.edition==FirmwareEdition::Score?"Score":"Factory")<<'-'
        <<std::hex<<std::uppercase<<std::setw(4)<<std::setfill('0')<<code<<'-'<<phase<<".png";
    const auto& display=b.machine.Current().display;
    return FirmwareBitmap::SavePngRgb565(path.str(),display.rgba.data(),display.width,display.height,b.error);
}
inline bool Click(Bench& b, bool plus) { return b.Hold(false,plus,!plus,120000,150000); }
inline bool Fire(Bench& b) { return b.Hold(true,false,false,120000,150000); }
inline bool Enter(Bench& b) {
    std::array<uint8_t,5> sequence{};
    unsigned step=0;
    for (bool plus : {true,false,true,false,true}) {
        if (!Click(b,plus)) return false;
        sequence[step++]=ObserveRegressionState(b.machine).menuSequence;
    }
    if (ObserveRegressionState(b.machine).menuActive==1) return true;
    std::ostringstream message;
    message<<"five SHORT released Plus Minus Plus Minus Plus did not open the native menu; sequence=";
    for (const auto value:sequence) message<<unsigned(value)<<',';
    return b.Require(false,message.str().c_str());
}
inline unsigned VisibleIndex(const FirmwareMenu::Settings& settings, FirmwareMenu::Item item) {
    unsigned index=0;
    for (const auto candidate:settings.order) {
        if (candidate!=FirmwareMenu::Item::Exit &&
            !(settings.enabledItems&(1u<<(unsigned(candidate)>2?unsigned(candidate)-1:unsigned(candidate))))) continue;
        if (candidate==item) return index;
        ++index;
    }
    return unsigned(FirmwareMenu::ItemCount);
}
inline unsigned ItemCount(const FirmwareMenu::Settings& settings) {
    return 1u+unsigned((settings.enabledItems&FirmwareMenu::InformationMask)!=0)+
        unsigned((settings.enabledItems&FirmwareMenu::VoltageMask)!=0)+
        unsigned((settings.enabledItems&FirmwareMenu::PuffEditorMask)!=0)+
        unsigned((settings.enabledItems&FirmwareMenu::ScoreEditorMask)!=0)+
        unsigned((settings.enabledItems&FirmwareMenu::RankSelectorMask)!=0);
}
inline bool Select(Bench& b, const FirmwareMenu::Settings& settings, FirmwareMenu::Item item) {
    const unsigned target=VisibleIndex(settings,item);
    for (unsigned i=0;i<ItemCount(settings) && ObserveRegressionState(b.machine).menuSelection!=target;++i)
        if (!Click(b,!settings.reverseNavigation)) return false;
    return b.Require(target<ItemCount(settings) && ObserveRegressionState(b.machine).menuSelection==target,
        "native menu did not expose the selected item at its configured visible position") && Fire(b);
}
inline bool Exit(Bench& b, const FirmwareMenu::Settings& settings={true,false}) {
    return Select(b,settings,FirmwareMenu::Item::Exit) && b.Advance(800000) &&
        b.Require(ObserveRegressionState(b.machine).menuActive==0,
            "Fire-select EXIT did not release the native menu capture barrier");
}
// Independent 5x7 decimal oracle; no display pixels or guest RAM are injected.
inline bool Number(Bench& b, uint32_t value, unsigned y) {
    constexpr std::array<std::array<uint8_t,5>,10> glyphs{{
        {{0x3e,0x51,0x49,0x45,0x3e}},{{0,0x42,0x7f,0x40,0}},{{0x42,0x61,0x51,0x49,0x46}},
        {{0x21,0x41,0x45,0x4b,0x31}},{{0x18,0x14,0x12,0x7f,0x10}},{{0x27,0x45,0x45,0x45,0x39}},
        {{0x3c,0x4a,0x49,0x49,0x30}},{{1,0x71,9,5,3}},{{0x36,0x49,0x49,0x49,0x36}},{{6,0x49,0x49,0x29,0x1e}}
    }};
    const auto text=std::to_string(value);
    const auto& pixels=b.machine.Current().display.rgba;
    if (pixels.size()!=80*160) return b.Require(false,"native menu has no physical LCD raster");
    for (size_t n=0;n<text.size();++n) for (unsigned x=0;x<6;++x) for (unsigned row=0;row<7;++row) {
        const unsigned column=34+unsigned(n)*6+x;
        if (column>=80) continue;
        const bool white=x<5 && (glyphs[size_t(text[n]-'0')][x]&(1u<<row));
        if (pixels[(y+row)*80+column]!=(white?0xffffffffu:0xff000000u))
            return b.Require(false,"native INFO decimal pixels do not equal the actual native RAM reading");
    }
    return true;
}
inline bool PreviewMatches(Bench& b, const FirmwareMenu::Settings& settings) {
    const auto state=ObserveRegressionState(b.machine);
    std::array<uint32_t,80*160> pixels;
    const FirmwareMenu::PreviewValues values{state.batteryMillivolts,state.resistanceMilliOhms,state.durationCentiseconds};
    const FirmwareMenu::PreviewState page{state.menuSelection,state.menuActive==2,state.menuBatteryVoltage};
    const auto& actual=b.machine.Current().display.rgba;
    if (!b.Require(FirmwareMenu::RenderPreview(settings,page,values,pixels) && actual.size()==pixels.size(),
        "native menu preview could not render the observed page")) return false;
    const auto mismatch=std::mismatch(pixels.begin(),pixels.end(),actual.begin());
    if (mismatch.first==pixels.end()) return true;
    const auto index=size_t(mismatch.first-pixels.begin());
    std::ostringstream detail;
    detail<<"Studio menu preview differs at ("<<index%80<<','<<index/80<<"): expected 0x"<<std::hex
        <<*mismatch.first<<", native 0x"<<*mismatch.second<<std::dec<<"; active="<<unsigned(state.menuActive)
        <<" selection="<<unsigned(state.menuSelection);
    return b.Require(false,detail.str().c_str());
}
inline bool NoOperationHints(Bench& b) {
    const auto& pixels=b.machine.Current().display.rgba;
    return b.Require(pixels.size()==80*160 &&
        std::all_of(pixels.begin()+110*80,pixels.begin()+117*80,
            [](uint32_t pixel) { return pixel==0xff000000u; }),
        "native menu still draws a button-operation hint");
}
inline bool MainSurface(Bench& b, const StudioState& source, uint16_t code) {
    const auto& settings=source.menuPatch;
    const auto original=b.machine.Current().display.rgba;
    const unsigned count=ItemCount(settings);
    if (!Capture(b,source,code,"main-before") || !Enter(b) || !PreviewMatches(b,settings) ||
        !NoOperationHints(b) || !Click(b,!settings.reverseNavigation) || !PreviewMatches(b,settings) ||
        !b.Require(ObserveRegressionState(b.machine).menuSelection==1,"configured down button did not move down") ||
        !Click(b,settings.reverseNavigation) || !PreviewMatches(b,settings) ||
        !b.Require(ObserveRegressionState(b.machine).menuSelection==0,"configured up button did not move up") ||
        !Click(b,settings.reverseNavigation) || !PreviewMatches(b,settings) ||
        !b.Require(ObserveRegressionState(b.machine).menuSelection==count-1,"configured up button did not wrap upward") ||
        !Click(b,!settings.reverseNavigation) || !PreviewMatches(b,settings) ||
        !b.Require(ObserveRegressionState(b.machine).menuSelection==0,"configured down button did not wrap downward")) return false;
    if (settings.enabledItems&FirmwareMenu::InformationMask) {
        if (!Select(b,settings,FirmwareMenu::Item::Information) ||
            !b.Require(ObserveRegressionState(b.machine).menuActive==2,"reordered INFO did not open its module")) return false;
        const auto info=ObserveRegressionState(b.machine);
        unsigned y=35;
        if ((settings.infoFields&FirmwareMenu::InfoBatteryMask) && !Number(b,info.batteryMillivolts,y)) return false;
        y+=20*unsigned((settings.infoFields&FirmwareMenu::InfoBatteryMask)!=0);
        if ((settings.infoFields&FirmwareMenu::InfoResistanceMask) && !Number(b,info.resistanceMilliOhms,y)) return false;
        y+=20*unsigned((settings.infoFields&FirmwareMenu::InfoResistanceMask)!=0);
        if ((settings.infoFields&FirmwareMenu::InfoDurationMask) && !Number(b,info.durationCentiseconds,y)) return false;
        if (!PreviewMatches(b,settings) || !NoOperationHints(b) || !Fire(b) ||
            !b.Require(ObserveRegressionState(b.machine).menuActive==1,"INFO did not return to the ordered menu")) return false;
    }
    if (settings.enabledItems&FirmwareMenu::VoltageMask) {
        if (!Select(b,settings,FirmwareMenu::Item::Voltage) ||
            !b.Require(ObserveRegressionState(b.machine).menuBatteryVoltage,"reordered VOLTAGE did not enable battery voltage") ||
            !Select(b,settings,FirmwareMenu::Item::Voltage) ||
            !b.Require(!ObserveRegressionState(b.machine).menuBatteryVoltage,"reordered VOLTAGE did not restore coil voltage")) return false;
    }
    return Exit(b,settings) && b.Advance(1200000) && Capture(b,source,code,"main-after") &&
        b.Require(b.machine.Current().display.rgba==original,
            "returning from the menu did not restore the clean native Main framebuffer");
}
inline bool Persistence(StudioState& source, uint16_t code, std::string& error) {
    Bench p;
    auto reopen=[&](const std::vector<uint8_t>& storage) {
        FirmwareDevice::Machine fresh;
        if(!fresh.Open(source,code,storage,p.error)) return false;
        p.machine=std::move(fresh); p.inputs={};
        return p.Advance(250000) && p.Boot();
    };
    auto run=[&]() {
        if(!reopen({})) return false;
        auto state=ObserveRegressionState(p.machine);
        if(!Click(p,Power(state)<state.powerLimit)) return false;
        const auto original=Power(ObserveRegressionState(p.machine));
        if(!p.FiveClicks() || !p.Advance(3000000)) return false;
        auto storage=p.machine.SaveStorage();
        if(!reopen(storage) || !p.Require(Power(ObserveRegressionState(p.machine))==original,
            "native OFF did not establish the pre-code persistent power baseline")) return false;
        std::array<FirmwareDevice::RegressionObservation,4> steps{};
        size_t step=0;
        for(bool plus:{true,false,true,false}) {
            if(!Click(p,plus)) return false;
            steps[step++]=ObserveRegressionState(p.machine);
        }
        p.inputs.plus=true; p.machine.SetInputs(p.inputs);
        if(!p.Advance(120000)) return false;
        p.inputs.plus=false; p.machine.SetInputs(p.inputs);
        const auto deadline=p.machine.Current().virtualMicroseconds+300000;
        while(!ObserveRegressionState(p.machine).menuActive && p.machine.Current().virtualMicroseconds<deadline)
            if(!p.Advance(1000)) return false;
        if(ObserveRegressionState(p.machine).menuActive!=1) {
            std::ostringstream detail;
            detail<<"persistence code never completed; recognition(sequence/savedPower/power)=";
            for(const auto& s:steps) detail<<unsigned(s.menuSequence)<<'/'<<s.menuSavedPower<<'/'<<Power(s)<<',';
            const auto s=ObserveRegressionState(p.machine);
            detail<<unsigned(s.menuSequence)<<'/'<<s.menuSavedPower<<'/'<<Power(s);
            return p.Require(false,detail.str().c_str());
        }
        if(!p.machine.Reset(p.error) || !p.Advance(250000) || !p.Boot() ||
            !p.Require(Power(ObserveRegressionState(p.machine))==original,
                "immediate reset after completed code restored a speculative Plus/Minus Flash power")) return false;
        if(!Enter(p) || !Exit(p) || !p.FiveClicks() || !p.Advance(3000000)) return false;
        storage=p.machine.SaveStorage();
        if(!reopen(storage) || !p.Require(Power(ObserveRegressionState(p.machine))==original,
            "native save/power-cycle persisted a speculative completed-code power")) return false;
        state=ObserveRegressionState(p.machine);
        const auto revision=p.machine.StorageRevision();
        if(!Click(p,Power(state)<state.powerLimit)) return false;
        const auto partial=Power(ObserveRegressionState(p.machine));
        // The stock periodic task sleeps 1000 ms and saves only after its
        // idle seconds counter exceeds 20; candidate expiry is not a save deadline.
        if(!p.Advance(1000000) || !p.Require(!ObserveRegressionState(p.machine).menuSequence,
            "partial candidate did not expire independently of native save eligibility") ||
            !p.Advance(24000000) || !p.Require(p.machine.StorageRevision()>revision,
                "expired partial candidate starved the eligible native asynchronous settings save") ||
            !p.FiveClicks() || !p.Advance(3000000)) return false;
        storage=p.machine.SaveStorage();
        return reopen(storage) && p.Require(Power(ObserveRegressionState(p.machine))==partial,
            "ordinary partial-code power no longer persisted through the native save path");
    };
    const bool ok=run();
    if(!ok) error=p.error+"; "+FirmwareDevice::DescribeRegressionState(p.machine);
    return ok;
}

inline bool NoHeat(Bench& b, uint32_t duration, uint32_t puffs) {
    uint32_t sustained=0;
    while (duration) {
        const auto slice=(std::min)(duration,10000u);
        if (!b.Advance(slice)) return false;
        const auto s=ObserveRegressionState(b.machine);
        if (!b.Require(s.menuActive && !(s.flags&1) && s.puffs==puffs,
            "physical Fire/PA4 automatic draw started a native puff while menu capture was active")) return false;
        sustained=s.pwmActive?sustained+slice:0;
        if (!b.Require(sustained<20000,"menu allowed sustained heater PWM, not a short native measurement probe")) return false;
        duration-=slice;
    }
    return true;
}
// Exercise real GPIO holds: event 2 must not pre-confirm the final digit/rank.
inline bool CancelHeldFire(Bench& b) {
    const auto before=ObserveRegressionState(b.machine);
    const auto revision=b.machine.StorageRevision();
    const auto storage=b.machine.SaveStorage();
    b.inputs.fire=true; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,450000,before.puffs)) return false;
    auto held=ObserveRegressionState(b.machine);
    if (!b.Require(held.menuActive==before.menuActive && held.menuPuffDigit==before.menuPuffDigit &&
        held.menuPuffValue==before.menuPuffValue && held.puffs==before.puffs &&
        held.scoreRawTotal==before.scoreRawTotal && b.machine.StorageRevision()==revision,
        "held Fire pre-confirmed an editor digit/rank or wrote Flash before cancellation")) return false;
    if (!NoHeat(b,500000,before.puffs)) return false;
    held=ObserveRegressionState(b.machine);
    if (!b.Require(held.menuActive==1 && !held.menuPuffValue && !held.menuPuffOriginal &&
        !held.menuPuffDigit && !held.menuPuffStatus,
        "700ms Fire did not discard the editor and return to root")) return false;
    b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,200000,before.puffs)) return false;
    b.inputs.fire=b.inputs.airflow=false; b.machine.SetInputs(b.inputs);
    if (!b.Advance(300000)) return false;
    const auto after=ObserveRegressionState(b.machine);
    return b.Require(after.menuActive==1 && after.menuSelection==before.menuSelection &&
        after.puffs==before.puffs && after.scoreRawTotal==before.scoreRawTotal &&
        after.scoreBestSinglePuff==before.scoreBestSinglePuff && Power(after)==Power(before) &&
        after.mode==before.mode && after.locked==before.locked && after.powerLimit==before.powerLimit &&
        b.machine.StorageRevision()==revision && b.machine.SaveStorage()==storage,
        "long Fire cancellation/release changed native values/Flash or re-entered the editor");
}
inline bool Envelope(StudioState& source, std::string& error) {
    std::vector<uint8_t> inner,outer;
    if (!FirmwareMenu::Settings{}.reverseNavigation) {
        error="new menu settings must default to physical Plus up / Minus down"; return false;
    }
    BuildReport report;
    if (!source.BuildZwdfBytes(inner,error)) return false;
    outer=inner;
    if (!FirmwareMenu::Apply(outer,{},report,error) || outer!=inner) { error="disabled menu changed stock firmware bytes"; return false; }
    if (!FirmwareMenu::Supported(source.firmware) || !FirmwareMenu::Apply(outer,{true,false},report,error)) return false;
    FirmwarePackage a,b;
    if (!FirmwarePackage::Parse(inner,a,error) || !FirmwarePackage::Parse(outer,b,error)) return false;
    for (size_t i=0;i<b.records.size();++i) {
        const auto& stock=a.records[i]; const auto& patched=b.records[i];
        const size_t footer=patched.image.size()-24, start=Word(patched.image.data()+footer+8);
        if (Word(patched.image.data()+start+8)!=1 || Word(patched.image.data()+footer+20)!=1) {
            error="native menu did not emit the public v1 envelope"; return false;
        }
        if (!FirmwareMenu::StateAddress(patched) || patched.sp!=stock.sp || patched.reset!=stock.reset ||
            patched.image.size()-stock.image.size()!=FirmwareMenu::FlashBytes(stock,{true,false}) ||
            uint64_t(patched.linked_base)+patched.image.size()>=0x0801f000) {
            error="native menu state ownership, exact Flash cost, original SP or protected Flash bounds are wrong"; return false;
        }
        if(FirmwareMenu::StateAddress(patched)<0x20000000 || uint64_t(FirmwareMenu::StateAddress(patched))+56>stock.sp) {
            error="reserved native TCB state exceeds the stock proved SRAM allocation"; return false;
        }
        auto normalized=patched;
        if (!FirmwareMenu::NormalizeForGuard(stock,normalized,error) || normalized.image!=stock.image) return false;
        for (unsigned kind=0;kind<5;++kind) {
            auto damaged=patched;
            const size_t footer=damaged.image.size()-24, start=Word(damaged.image.data()+footer+8);
            const size_t at=kind==0?start+64:kind==1?footer+8:kind==2?start+28:kind==3?stock.image.size()-1:footer;
            damaged.image[at]^=1;
            if (kind==0 || kind==2) Word(damaged.image,footer+16,
                FirmwarePackage::Crc16Ccitt(damaged.image.data()+start,footer-start));
            std::string rejected;
            if (FirmwareMenu::StateAddress(damaged) || FirmwareMenu::NormalizeForGuard(stock,damaged,rejected)) {
                error="native menu accepted tampered executor/state/footer/inner bytes"; return false;
            }
        }
    }
    if (!Variants(inner,a,b,error) || !FieldVariants(inner,a,error)) return false;
    for (uint32_t version:{2u,3u,4u}) for (size_t i=0;i<b.records.size();++i) {
        auto unsupported=b.records[i];
        const size_t footer=unsupported.image.size()-24, start=Word(unsupported.image.data()+footer+8);
        Word(unsupported.image,start+8,version);
        Word(unsupported.image,footer+20,version);
        Word(unsupported.image,footer+16,
            FirmwarePackage::Crc16Ccitt(unsupported.image.data()+start,footer-start));
        std::string rejected;
        if (FirmwareMenu::StateAddress(unsupported) ||
            FirmwareMenu::NormalizeForGuard(a.records[i],unsupported,rejected)) {
            error="native menu accepted an obsolete development envelope version"; return false;
        }
    }
    FirmwareMenu::Settings extracted;
    if (!FirmwareMenu::Extract(b,extracted,error) || extracted!=FirmwareMenu::Settings{true,false}) return false;
    for (size_t i=0;i<a.records.size();++i) if (a.records[i].image!=b.records[i].image) {
        error="native menu extraction failed to restore the exact inner firmware"; return false;
    }
    // Extra creator references OUTSIDE the immutable known caller blocks must
    // fail too: merely checking old callsites would not establish the capacity.
    constexpr std::array<std::pair<uint16_t,uint32_t>,7> scoreCreators{{{0x3013,0x0800d730},{0x3014,0x0800d740},
        {0x3029,0x0800d0dc},{0x302a,0x0800d0ec},{0x3103,0x0800f980},{0x3105,0x0800fab8},{0x314d,0x0800f8ec}}};
    constexpr std::array<std::pair<uint16_t,uint32_t>,9> factoryCreators{{{0x3013,0x0800c548},{0x3014,0x0800c558},
        {0x3029,0x0800ba18},{0x3034,0x0800bb94},{0x3148,0x0800b9b0},{0x3103,0x0800f530},
        {0x3104,0x08010108},{0x3105,0x08010760},{0x314d,0x0800fe80}}};
    for (size_t i=0;i<a.records.size();++i) {
        auto modified=a;
        const uint16_t code=uint16_t(a.records[i].device_code[0])<<8|a.records[i].device_code[1];
        uint32_t creator=0;
        if (source.edition==FirmwareEdition::Score) { for (auto [c,p]:scoreCreators) if(c==code) creator=p; }
        else { for (auto [c,p]:factoryCreators) if(c==code) creator=p; }
        Word(modified.records[i].image,0x1c,creator|1);
        if (FirmwareMenu::Supported(modified)) { error="new task creator outside known guarded blocks was accepted"; return false; }
        std::vector<uint8_t> bytes;
        if (!modified.PackWith({}, {}, bytes,report,error)) return false;
        const auto untouched=bytes;
        std::string rejected;
        if (FirmwareMenu::Apply(bytes,{true,false},report,rejected) || bytes!=untouched) {
            error="Apply accepted an additional native task creator or changed rejected output"; return false;
        }
    }
    // Custom screens stay the exact INNER envelope, byte for byte.
    auto logos=source.screenPatches;
    logos[size_t(FirmwareLogo::Target::DeviceInfo)]=DeviceCustomInfoRegressionDetail::Settings(true);
    if (!FirmwareLogo::Build(source.firmware,logos,{}, {},inner,report,error)) return false;
    outer=inner;
    if (!FirmwareMenu::Apply(outer,{true,true},report,error) || !FirmwarePackage::Parse(outer,b,error) ||
        !FirmwareMenu::Extract(b,extracted,error) || extracted!=FirmwareMenu::Settings{true,true}) return false;
    if (!FirmwarePackage::Parse(inner,a,error)) return false;
    for (size_t i=0;i<a.records.size();++i) if (a.records[i].image!=b.records[i].image) {
        error="menu changed the strict inner native screen envelope"; return false;
    }
    FirmwareLogo::Patches restored;
    if (!FirmwareLogo::Canonicalize(b,restored,error) || restored!=logos) return false;
    return true;
}
inline bool Row(StudioState& source, const HardwareRecord& record, std::string& error) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    Bench stock,b;
    source.menuPatch={};
    if (!stock.machine.Open(source,code,{},error) || !stock.Advance(250000) || !stock.Boot() || !stock.Advance(800000)) {
        if(error.empty()) error=stock.error; return false;
    }
    source.menuPatch={true,false};
    if (!b.machine.Open(source,code,{},error) || !b.Advance(250000) || !b.Boot() || !b.Advance(800000)) {
        if(error.empty()) error=b.error; return false;
    }
    auto run=[&]() {
        const auto original=ObserveRegressionState(stock.machine), enabled=ObserveRegressionState(b.machine);
        if (!b.Require(enabled.menuStateAddress && enabled.menuMagic==0x4d454e55 && !enabled.menuActive &&
            enabled.mode==original.mode && enabled.powerA==original.powerA && enabled.powerB==original.powerB &&
            enabled.puffs==original.puffs && b.machine.Current().display.rgba==stock.machine.Current().display.rgba,
            "enabled-but-unused menu changed the native default Main, power, mode or counters")) return false;
        if(!MainSurface(b,source,code)) return false;
        if(!Persistence(source,code,b.error)) return false;
        for (unsigned n=0;n<4;++n) if (!Click(b,n%2==0)) return false;
        if (!b.Require(!ObserveRegressionState(b.machine).menuActive,"partial four-key code was consumed as a menu")) return false;
        if (!b.Advance(1000000) || !Click(b,true) || !b.Require(!ObserveRegressionState(b.machine).menuActive,
            "unrelated keys beyond the inter-key timeout unexpectedly completed an old code")) return false;
        if (!b.Hold(false,true,false,1800000,300000) || !b.Require(!ObserveRegressionState(b.machine).menuActive,
            "long/repeating Plus was accepted as a SHORT code")) return false;
        if (!b.Hold(true,false,false,600000,1000000)) return false;
        // Select an even dual-input bank through native gestures, then prove
        // PA4 really starts a puff OUTSIDE the menu (not an inert test input).
        for (unsigned bank=0;bank<4 && ObserveRegressionState(b.machine).mode!=2;++bank) {
            for(unsigned click=0;click<3;++click) if(!b.Hold(true,false,false,70000,70000)) return false;
            if(!b.Advance(800000)) return false;
        }
        const auto autoBefore=ObserveRegressionState(b.machine);
        b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
        if(!b.Advance(600000)) return false;
        b.inputs.airflow=false; b.machine.SetInputs(b.inputs);
        if(!b.Advance(1000000)) return false;
        const auto autoAfter=ObserveRegressionState(b.machine);
        if(!b.Require(autoAfter.pwmStarts>autoBefore.pwmStarts && autoAfter.puffs==autoBefore.puffs+1 &&
            !autoAfter.pwmCompare,"physical PA4 did not execute native automatic-draw start/stop outside the menu")) return false;
        auto before=ObserveRegressionState(b.machine);
        if (!Enter(b) || !b.Require(Power(ObserveRegressionState(b.machine))==Power(before),
            "successful code did not restore the pre-sequence validated power exactly")) return false;
        if(!Capture(b,source,code,"menu")) return false;
        if(!b.Advance(35000000) || !b.Require(!DeviceFunctionalMatrixDetail::Visible(b.machine),
            "idle menu rendering kept the native activity timer/display awake") ||
            !Click(b,true) || !Click(b,false) || !b.Require(DeviceFunctionalMatrixDetail::Visible(b.machine) &&
                ObserveRegressionState(b.machine).menuSelection==0,"physical menu input did not wake the native display")) return false;
        const auto puffs=before.puffs;
        b.inputs.fire=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,600000,puffs)) return false;
        b.inputs.fire=false; b.machine.SetInputs(b.inputs);
        if (!b.Advance(300000) || !b.Require(ObserveRegressionState(b.machine).menuActive==2,
            "released physical Fire did not select INFO")) return false;
        auto info=ObserveRegressionState(b.machine);
        if (!Number(b,info.batteryMillivolts,35) || !Number(b,info.resistanceMilliOhms,55) ||
            !Number(b,info.durationCentiseconds,75)) return false;
        if(!Capture(b,source,code,"info")) return false;
        b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,700000,puffs)) return false;
        b.inputs.fire=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,600000,puffs)) return false;
        b.inputs.fire=false; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,300000,puffs)) return false;
        b.inputs.airflow=false; b.machine.SetInputs(b.inputs);
        if (!b.Advance(300000) || !b.Require(ObserveRegressionState(b.machine).menuActive==1,
            "Fire-back with automatic draw held escaped native menu capture")) return false;
        if (!Select(b,source.menuPatch,FirmwareMenu::Item::Voltage) ||
            !b.Require(ObserveRegressionState(b.machine).menuBatteryVoltage,
            "VOLTAGE Fire-select did not change the resident voltage setting")) return false;
        if (!Exit(b)) return false;
        auto battery=ObserveRegressionState(b.machine);
        if (!b.Require(battery.mode==before.mode && battery.puffs==puffs && Power(battery)==Power(before) &&
            battery.voltageRenders && battery.voltageCentivolts==battery.batteryMillivolts/10,
            "menu Fire selections leaked a delayed native click/puff/mode event, or Main did not render actual battery voltage")) return false;
        if(!Capture(b,source,code,"main-battery")) return false;
        const auto pixels=b.machine.Current().display.rgba;
        b.inputs.batteryMillivolts=3850; b.machine.SetInputs(b.inputs);
        if (!b.Advance(1200000)) return false;
        auto changed=ObserveRegressionState(b.machine);
        if (!b.Require(changed.voltageRenders>battery.voltageRenders && changed.voltageCentivolts==changed.batteryMillivolts/10 &&
            changed.batteryMillivolts!=battery.batteryMillivolts && b.machine.Current().display.rgba!=pixels,
            "idle Main battery readout was stale or did not use native ADC voltage/native pixels")) return false;
        if (!Enter(b) || !Fire(b) || !b.Advance(300000)) return false;
        info=ObserveRegressionState(b.machine);
        b.inputs.batteryMillivolts=4000; b.machine.SetInputs(b.inputs);
        if (!b.Advance(1200000)) return false;
        auto live=ObserveRegressionState(b.machine);
        if (!Number(b,live.batteryMillivolts,35) || !b.Require(live.batteryMillivolts!=info.batteryMillivolts,
            "INFO did not follow changing native battery data")) return false;
        if (!Fire(b) || !Select(b,source.menuPatch,FirmwareMenu::Item::Voltage) || !Exit(b)) return false;
        if (!b.Require(!ObserveRegressionState(b.machine).menuBatteryVoltage && ObserveRegressionState(b.machine).voltageCentivolts==0,
            "OUTPUT choice did not restore the stock Main voltage source")) return false;
        // Native simultaneous gestures remain stock, not custom-menu shortcuts.
        if (!b.Hold(false,true,true,1150000,300000)) return false;
        if (before.historyMenu) {
            if (!b.Require(ObserveRegressionState(b.machine).page==5,"Plus+Minus lost native history") ||
                !b.Hold(true,false,false,1600000,1000000)) return false;
        } else if (!b.Require(!ObserveRegressionState(b.machine).menuActive,"simultaneous Plus+Minus opened the sequential menu")) return false;
        const auto mode=ObserveRegressionState(b.machine).mode;
        for(unsigned i=0;i<3;++i) if(!b.Hold(true,false,false,70000,70000)) return false;
        if(!b.Advance(800000) || !b.Require(ObserveRegressionState(b.machine).mode==(mode+1)%4,
            "fresh native three-Fire gesture failed after custom-menu click cancellation")) return false;
        if(!b.Hold(true,true,false,1150000,500000) || !b.Require(ObserveRegressionState(b.machine).locked,
            "native Fire+Plus lock gesture was lost")) return false;
        const auto lockedPower=Power(ObserveRegressionState(b.machine));
        if(!Click(b,true) || !b.Require(Power(ObserveRegressionState(b.machine))==lockedPower,
            "ordinary Plus changed power under native lock")) return false;
        if(!Enter(b) || !b.Require(Power(ObserveRegressionState(b.machine))==lockedPower,
            "locked Main menu entry changed native power") || !Exit(b)) return false;
        if(!b.Hold(true,true,false,1150000,1000000) || !b.Require(!ObserveRegressionState(b.machine).locked,
            "native Fire+Plus unlock gesture was lost")) return false;
        if(!b.Hold(true,false,true,1150000,1000000) || !b.Require(!ObserveRegressionState(b.machine).puffs,
            "native Fire+Minus did not clear actual puffs after menu use")) return false;
        if(before.historyMenu) {
            if(!b.Hold(true,true,true,1150000,300000) || !b.Require(ObserveRegressionState(b.machine).page==6,
                "all-three native clock gesture was replaced by custom menu") || !Click(b,true) ||
                !b.Hold(true,false,false,1600000,1000000)) return false;
        }
        // At the native clamp, entry must restore the original value, not +/-1.
        for(unsigned i=0;i<85 && Power(ObserveRegressionState(b.machine))>5;++i) if(!Click(b,false)) return false;
        before=ObserveRegressionState(b.machine);
        if (!Enter(b) || !b.Require(Power(ObserveRegressionState(b.machine))==Power(before),"code at lower power clamp changed native power") || !Exit(b)) return false;
        for(unsigned i=0;i<85 && Power(ObserveRegressionState(b.machine))<ObserveRegressionState(b.machine).powerLimit;++i)
            if(!Click(b,true)) return false;
        before=ObserveRegressionState(b.machine);
        if (!Enter(b) || !b.Require(Power(ObserveRegressionState(b.machine))==Power(before),"code at upper power clamp changed native power") || !Exit(b)) return false;
        if (!b.FiveClicks() || !b.Advance(3000000) || !b.Require(!ObserveRegressionState(b.machine).railPowered ||
            !(ObserveRegressionState(b.machine).flags&0x400),"native five-Fire OFF gesture was lost after menu exit")) return false;
        if (!b.machine.Reset(b.error) || !b.Advance(250000) || !b.Boot() ||
            !b.Require(!ObserveRegressionState(b.machine).menuBatteryVoltage,"runtime menu choice survived battery-removal reset instead of configured initial selection")) return false;
        source.menuPatch={true,true};
        Bench configured;
        if(!configured.machine.Open(source,code,{},b.error) || !configured.Advance(250000) || !configured.Boot()) {
            if(b.error.empty()) b.error=configured.error; return false;
        }
        if(!b.Require(ObserveRegressionState(configured.machine).menuBatteryVoltage,
            "Studio initial battery voltage setting was not bundled into native firmware")) return false;
        source.screenPatches[size_t(FirmwareLogo::Target::DeviceInfo)]=DeviceCustomInfoRegressionDetail::Settings(true);
        Bench custom;
        if(!custom.machine.Open(source,code,{},b.error) || !custom.Advance(250000) || !custom.Boot()) {
            if(b.error.empty()) b.error=custom.error; return false;
        }
        uint64_t enteredAt=0;
        if(!DeviceCustomInfoRegressionDetail::Enter(custom,enteredAt) || !custom.Advance(500000) ||
            !b.Require(custom.machine.Current().display.rgba==DeviceCustomInfoRegressionDetail::Image(),
                "native custom CHIP ID Logo v8 pixels were replaced by the inactive menu") ||
            !custom.Hold(true,false,false,1600000,1000000) ||
            !b.Require(ObserveRegressionState(custom.machine).page==1 && !ObserveRegressionState(custom.machine).menuActive,
                "custom native CHIP ID close did not return to ordinary Main")) {
            if(b.error.empty()) b.error=custom.error; return false;
        }
        source.screenPatches[size_t(FirmwareLogo::Target::DeviceInfo)]={};
        return true;
    };
    const bool ok=run();
    if(!ok) error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    source.menuPatch={};
    source.screenPatches[size_t(FirmwareLogo::Target::DeviceInfo)]={};
    return ok;
}
} // namespace DeviceMenuRegressionDetail

inline bool RunDeviceMenuRegressions(std::string& error, std::ostream* report=nullptr) {
    error.clear(); std::ostringstream failures; size_t visited=0,failed=0;
    for(auto edition:{FirmwareEdition::Score,FirmwareEdition::Factory}) {
        StudioState source; FirmwarePackage package; std::string detail;
        if(!LoadEmbeddedFirmware(package,detail,FirmwareModel::DragS,edition) ||
            !source.LoadPackage(std::move(package),"native physical menu regressions",detail) ||
            !DeviceMenuRegressionDetail::Envelope(source,detail)) {
            failures<<(edition==FirmwareEdition::Score?"Score":"Factory")<<"/envelope: "<<detail<<'\n'; ++failed; continue;
        }
        for(const auto& record:source.firmware.records) {
            const auto code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            ++visited;
            const bool ok=DeviceMenuRegressionDetail::Row(source,record,detail);
            if(!ok) { ++failed; failures<<"HW "<<std::hex<<code<<std::dec<<": "<<detail<<'\n'; }
            if(report) *report<<(edition==FirmwareEdition::Score?"Score":"Factory")<<"/HW "<<std::hex<<code<<std::dec
                <<": "<<(ok?"PASS":"FAIL")<<' '<<(ok?"physical menu, native data/SPI, Fire+airflow inhibit, original gestures":detail)<<'\n'<<std::flush;
        }
    }
    if(visited!=16) { ++failed; failures<<"Expected all 7 Score + 9 Factory native records; visited "<<visited<<'\n'; }
    if(report) *report<<"DEVICE MENU REGRESSIONS "<<(failed?"FAILED":"OK")<<" records="<<visited<<" failures="<<failed<<'\n'<<std::flush;
    error=failures.str(); return failed==0;
}

inline bool RunDeviceMenuFactory3014Regressions(std::string& error, std::ostream* report=nullptr) {
    StudioState source;
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,FirmwareEdition::Factory) ||
        !source.LoadPackage(std::move(package),"Factory HW3014 voltage entry regression",error) ||
        !DeviceMenuRegressionDetail::Envelope(source,error)) return false;
    for (const auto& record:source.firmware.records) {
        if (record.device_code[0] != 0x30 || record.device_code[1] != 0x14) continue;
        const bool ok=DeviceMenuRegressionDetail::Row(source,record,error);
        if (report) *report << "FACTORY HW3014 MENU REGRESSIONS " << (ok?"OK":"FAILED") << '\n';
        return ok;
    }
    error="Factory HW3014 regression record is missing";
    return false;
}

inline bool RunDeviceMenuMainSurfaceRegressions(std::string& error, std::ostream* report=nullptr) {
    unsigned visited=0;
    for (auto edition:{FirmwareEdition::Score,FirmwareEdition::Factory}) {
        StudioState source;
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,edition) ||
            !source.LoadPackage(std::move(package),"Native menu Main redraw regression",error) ||
            !DeviceMenuRegressionDetail::Envelope(source,error)) return false;
        for (const auto& record:source.firmware.records) {
            const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            for (unsigned variant=0;variant<5;++variant) {
                source.menuPatch={true,false};
                if (variant==1) {
                    source.menuPatch.reverseNavigation=false;
                    source.menuPatch.order={FirmwareMenu::Item::PuffEditor,FirmwareMenu::Item::Exit,
                        FirmwareMenu::Item::Voltage,FirmwareMenu::Item::Information,
                        FirmwareMenu::Item::ScoreEditor,FirmwareMenu::Item::RankSelector};
                } else if (variant==2) {
                    source.menuPatch.reverseNavigation=false;
                    source.menuPatch.enabledItems=FirmwareMenu::InformationMask;
                    source.menuPatch.order={FirmwareMenu::Item::Voltage,FirmwareMenu::Item::Information,
                        FirmwareMenu::Item::PuffEditor,FirmwareMenu::Item::Exit,
                        FirmwareMenu::Item::ScoreEditor,FirmwareMenu::Item::RankSelector};
                    source.menuPatch.infoFields=1u+visited%FirmwareMenu::AllInfoFields;
                } else if (variant==3) {
                    source.menuPatch.enabledItems=FirmwareMenu::VoltageMask;
                    source.menuPatch.order={FirmwareMenu::Item::PuffEditor,FirmwareMenu::Item::Exit,
                        FirmwareMenu::Item::Information,FirmwareMenu::Item::Voltage,
                        FirmwareMenu::Item::ScoreEditor,FirmwareMenu::Item::RankSelector};
                    source.menuPatch.infoFields=0;
                } else if (variant==4) {
                    source.menuPatch.reverseNavigation=true;
                    source.menuPatch.infoFields=1u+visited%FirmwareMenu::AllInfoFields;
                }
                DeviceFunctionalMatrixDetail::Bench b;
                if (!b.machine.Open(source,code,{},error) || !b.Advance(250000) || !b.Boot() ||
                    !b.Advance(800000) || !DeviceMenuRegressionDetail::MainSurface(b,source,code)) {
                    if (error.empty()) error=b.error;
                    error+="; HW "+std::to_string(code)+"; variant "+std::to_string(variant)+"; "+
                        FirmwareDevice::DescribeRegressionState(b.machine);
                    return false;
                }
            }
            ++visited;
            if (report) *report<<(edition==FirmwareEdition::Score?"Score":"Factory")<<"/HW "<<std::hex<<code
                <<std::dec<<": PASS native directions, field subsets, configured order and exact preview pixels\n"<<std::flush;
        }
    }
    if (visited!=16) { error="Native Main redraw did not cover all sixteen revisions"; return false; }
    if (report) *report<<"NATIVE MENU MAIN SURFACE REGRESSIONS OK records=16 configurations=80\n";
    return true;
}
