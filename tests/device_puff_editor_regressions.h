#pragma once
#include "device_menu_regressions.h"

namespace DevicePuffEditorRegressionDetail {
using namespace DeviceMenuRegressionDetail;
inline bool Raster(Bench& b, const StudioState& source, uint32_t limit) {
    const auto observed=ObserveRegressionState(b.machine);
    FirmwareMenu::PreviewState page;
    page.selection=observed.menuSelection;
    page.batteryVoltage=observed.menuBatteryVoltage;
    page.editingPuffs=true;
    page.puffDigit=observed.menuPuffDigit;
    page.puffValue=observed.menuPuffValue;
    page.originalPuffs=observed.menuPuffOriginal;
    FirmwareMenu::PreviewValues values;
    page.puffStatus=observed.menuPuffStatus;
    values.puffs=observed.puffs;
    values.maximumPuffs=limit;
    std::array<uint32_t,80*160> expected;
    const auto& actual=b.machine.Current().display.rgba;
    if (!b.Require(FirmwareMenu::RenderPreview(source.menuPatch,page,values,expected) && actual.size()==expected.size(),
        "puff editor preview cannot render the physical editor state")) return false;
    if (page.puffStatus!=1 && !NoOperationHints(b)) return false;
    const auto mismatch=std::mismatch(expected.begin(),expected.end(),actual.begin());
    if (mismatch.first==expected.end()) return true;
    const auto at=size_t(mismatch.first-expected.begin());
    std::ostringstream detail;
    detail<<"puff editor native/preview pixel mismatch at "<<at%80<<','<<at/80<<" digit="<<observed.menuPuffDigit;
    return b.Require(false,detail.str().c_str());
}
inline bool OpenEditor(Bench& b, const StudioState& source, uint32_t limit) {
    if (!ObserveRegressionState(b.machine).menuActive && !Enter(b)) return false;
    const auto current=ObserveRegressionState(b.machine).puffs;
    return Select(b,source.menuPatch,FirmwareMenu::Item::PuffEditor) &&
        b.Require(ObserveRegressionState(b.machine).menuActive==4 &&
            ObserveRegressionState(b.machine).menuPuffDigit==0 &&
            ObserveRegressionState(b.machine).menuPuffValue==current &&
            ObserveRegressionState(b.machine).menuPuffOriginal==current &&
            ObserveRegressionState(b.machine).menuPuffStatus==(current>FirmwareMenu::PuffEditorMaximum?2u:0u),
            "puff editor did not load the raw counter with the first of five digits selected") && Raster(b,source,limit);
}
inline bool FinishUnchanged(Bench& b, uint32_t current, unsigned firstDigit=0) {
    const auto revision=b.machine.StorageRevision();
    for (unsigned digit=firstDigit;digit<FirmwareMenu::PuffDigitCount;++digit) {
        if (!Fire(b)) return false;
        const auto state=ObserveRegressionState(b.machine);
        if (!b.Require(state.puffs==current,"unconfirmed digit navigation changed the native counter")) return false;
        if (digit+1<FirmwareMenu::PuffDigitCount && !b.Require(state.menuActive==4 && state.menuPuffDigit==digit+1,
            "Fire did not advance exactly one decimal digit")) return false;
    }
    return b.Require(ObserveRegressionState(b.machine).menuActive==1 && b.machine.StorageRevision()==revision,
        "unchanged puff editor confirmation wrote Flash or failed to return to the menu");
}
// Reach the range-error screen through the real split-counter native increment,
// never a host RAM injection or widened editor limit.
inline bool RangeError(Bench& b, const StudioState& source, uint32_t limit) {
    if (!OpenEditor(b,source,limit)) return false;
    constexpr uint32_t places[]{10000,1000,100,10,1};
    for (unsigned digit=0;digit<FirmwareMenu::PuffDigitCount;++digit) {
        for (unsigned clicks=0;clicks<10 &&
            ObserveRegressionState(b.machine).menuPuffValue/places[digit]%10!=9;++clicks)
            if (!Click(b,true)) return false;
        if (!b.Require(ObserveRegressionState(b.machine).menuPuffValue/places[digit]%10==9 &&
            ObserveRegressionState(b.machine).menuPuffValue<=limit,
            "five-digit upper bound could not be staged without exceeding the editor limit") ||
            !Raster(b,source,limit) || !Fire(b)) return false;
    }
    if (!b.Require(ObserveRegressionState(b.machine).puffs==99999 &&
        ObserveRegressionState(b.machine).menuActive==1,
        "fifth Fire did not commit the exact five-digit upper bound") ||
        !Exit(b,source.menuPatch) || !b.machine.Reset(b.error) || !b.Advance(250000) || !b.Boot() ||
        !b.Advance(800000) || !b.Require(ObserveRegressionState(b.machine).puffs==99999,
            "the five-digit upper bound did not survive a native Flash reset") ||
        !b.Hold(true,false,false,500000,700000) ||
        !b.Require(ObserveRegressionState(b.machine).puffs==100000,
            "raw split counter was clamped to the editor's five-digit bound")) return false;
    const auto revision=b.machine.StorageRevision();
    if (!OpenEditor(b,source,limit) || !b.Hold(false,true,false,1200000,300000) ||
        !b.Hold(false,true,true,450000,300000) || !Click(b,true) || !Click(b,false) ||
        !b.Require(ObserveRegressionState(b.machine).menuPuffValue==100000 &&
            ObserveRegressionState(b.machine).menuPuffOriginal==100000 &&
            ObserveRegressionState(b.machine).menuPuffDigit==0 &&
            ObserveRegressionState(b.machine).menuPuffStatus==2 &&
            ObserveRegressionState(b.machine).puffs==100000 && b.machine.StorageRevision()==revision,
            "COUNT TOO BIG allowed editing, truncated the raw counter, or wrote storage") ||
        !Raster(b,source,limit) || !Fire(b)) return false;
    return b.Require(ObserveRegressionState(b.machine).menuActive==1 &&
        ObserveRegressionState(b.machine).puffs==100000 && b.machine.StorageRevision()==revision,
        "range-error Fire-back changed the raw counter or persisted a truncated value");
}
inline bool HostPreview(std::string& error) {
    FirmwareMenu::Settings settings{true,false};
    settings.enabledItems=FirmwareMenu::PuffEditorMask;
    settings.infoFields=0;
    FirmwareMenu::PreviewState page;
    page.editingPuffs=true;
    page.puffValue=99999;
    FirmwareMenu::PreviewValues values;
    values.maximumPuffs=999999; // Raw physical bounds are capped only for editing.
    std::array<uint32_t,80*160> pixels, expected;
    for (unsigned digit=0;digit<5;++digit) {
        page.puffDigit=digit;
        if (!FirmwareMenu::RenderPreview(settings,page,values,pixels)) {
            error="five-digit puff preview rejected a valid digit"; return false;
        }
        for (unsigned y=72;y<74;++y) for (unsigned x=0;x<80;++x)
            if (pixels[y*80+x]!=(x>=11+digit*12 && x<21+digit*12?0xffffffffu:0xff000000u)) {
                error="five-digit underline is not centered at the native 11px origin and 12px pitch"; return false;
            }
    }
    values.maximumPuffs=99999;
    if (!FirmwareMenu::RenderPreview(settings,page,values,expected) || pixels!=expected) {
        error="puff preview displayed a raw six-digit bound instead of the effective editor limit"; return false;
    }
    page.puffDigit=5;
    if (FirmwareMenu::RenderPreview(settings,page,values,pixels)) {
        error="five-digit preview accepted a sixth selected digit"; return false;
    }
    page.puffDigit=0;
    page.puffValue=100000;
    if (!FirmwareMenu::RenderPreview(settings,page,values,pixels)) {
        error="raw out-of-range puff preview could not render"; return false;
    }
    page.puffStatus=2;
    if (!FirmwareMenu::RenderPreview(settings,page,values,expected) || pixels!=expected ||
        !std::all_of(pixels.begin()+72*80,pixels.begin()+74*80,
            [](uint32_t pixel) { return pixel==0xff000000u; })) {
        error="out-of-range preview rendered truncated digits instead of COUNT TOO BIG"; return false;
    }
    return true;
}
inline bool Envelope(StudioState& source, std::string& error) {
    if (!HostPreview(error)) return false;
    std::vector<uint8_t> inner;
    if (!source.BuildZwdfBytes(inner,error)) return false;
    FirmwarePackage original;
    if (!FirmwarePackage::Parse(inner,original,error)) return false;
    for (uint32_t modules=FirmwareMenu::PuffEditorMask;modules<=(FirmwareMenu::DefaultMask|FirmwareMenu::PuffEditorMask);++modules) {
        const bool info=(modules&FirmwareMenu::InformationMask)!=0;
        for (uint32_t fields=info?1u:0u;fields<=(info?FirmwareMenu::AllInfoFields:0u);++fields) {
            FirmwareMenu::Settings settings{true,false};
            settings.enabledItems=modules;
            settings.infoFields=fields;
            auto bytes=inner;
            BuildReport report;
            FirmwarePackage package;
            if (!FirmwareMenu::Apply(bytes,settings,report,error) || !FirmwarePackage::Parse(bytes,package,error)) return false;
            for (size_t i=0;i<package.records.size();++i) {
                const auto& image=package.records[i].image;
                const size_t footer=image.size()-24, start=Word(image.data()+footer+8);
                if (Word(image.data()+start+8)!=1 || Word(image.data()+footer+20)!=1) {
                    error="puff editor did not emit the full public v1 menu envelope"; return false;
                }
                const auto cost=FirmwareMenu::FlashCosts(original.records[i],settings);
                if (!cost.puffEditor || cost.total!=package.records[i].image.size()-original.records[i].image.size() ||
                    cost.common+cost.information+cost.voltage+cost.puffEditor!=cost.total) {
                    error="optional puff editor bytes are absent, duplicated, or not predicted exactly"; return false;
                }
            }
            FirmwareMenu::Settings restored;
            if (!FirmwareMenu::Extract(package,restored,error) || restored!=settings) return false;
            for (size_t i=0;i<package.records.size();++i) if (package.records[i].image!=original.records[i].image) {
                error="puff editor envelope did not restore the exact native inner firmware"; return false;
            }
        }
    }
    return true;
}
inline bool Row(StudioState& source, const HardwareRecord& record, std::string& error) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const auto limit=FirmwareMenu::PuffLimit(record);
    Bench b;
    source.menuPatch={true,false};
    source.menuPatch.enabledItems=FirmwareMenu::PuffEditorMask;
    source.menuPatch.infoFields=0;
    const auto run=[&]() {
        const auto expectedLimit=source.edition==FirmwareEdition::Score && (code==0x3013 || code==0x3014)?9999u:99999u;
        if (!b.Require(limit==expectedLimit,"effective puff editor bound differs from the native five-digit bound") ||
            !b.machine.Open(source,code,{},b.error) || !b.Advance(250000) || !b.Boot() || !b.Advance(800000) ||
            !OpenEditor(b,source,limit)) return false;
        const auto initial=ObserveRegressionState(b.machine);
        if (!Capture(b,source,code,"puffs-initial")) return false;
        const auto leadingMaximum=limit/10000;
        for (unsigned click=1;click<=leadingMaximum+1;++click)
            if (!Click(b,true) || !b.Require(ObserveRegressionState(b.machine).menuPuffValue==
                (click%(leadingMaximum+1))*10000 && ObserveRegressionState(b.machine).puffs==initial.puffs,
                "leading digit did not wrap at the effective native editor bound")) return false;
        if (!Click(b,false) || !b.Require(ObserveRegressionState(b.machine).menuPuffValue==leadingMaximum*10000,
            "Minus did not wrap the first digit to its largest valid value") ||
            !Click(b,true) || !b.Require(ObserveRegressionState(b.machine).menuPuffValue==initial.puffs &&
                ObserveRegressionState(b.machine).menuPuffDigit==0,
                "leading-digit wrap changed another digit or selected position")) return false;
        b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,200000,initial.puffs)) return false;
        b.inputs.airflow=false;
        b.inputs.fire=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,600000,initial.puffs)) return false;
        b.inputs.fire=false; b.machine.SetInputs(b.inputs);
        if (!b.Advance(300000) || !b.Require(ObserveRegressionState(b.machine).menuPuffDigit==1,
            "released Fire did not advance the editor after a held no-heat check") ||
            !FinishUnchanged(b,initial.puffs,1) || !OpenEditor(b,source,limit)) return false;
        // A net-zero edit must not consume a Flash erase/program cycle.
        for (unsigned digit=0;digit+1<FirmwareMenu::PuffDigitCount;++digit) if (!Fire(b)) return false;
        if (!Click(b,false) || !b.Require(ObserveRegressionState(b.machine).menuPuffValue==9,
            "Minus did not wrap the selected zero digit to nine") || !Raster(b,source,limit) ||
            !Click(b,true) || !b.Require(ObserveRegressionState(b.machine).menuPuffValue==initial.puffs,
                "Plus did not wrap nine to zero without changing another position") ||
            !FinishUnchanged(b,initial.puffs,FirmwareMenu::PuffDigitCount-1) || !OpenEditor(b,source,limit)) return false;
        for (unsigned digit=0;digit+1<FirmwareMenu::PuffDigitCount;++digit) if (!Fire(b)) return false;
        if (!Click(b,true) || !CancelHeldFire(b) || !OpenEditor(b,source,limit)) return false;
        const uint32_t target=limit==99999?87654:9876;
        constexpr uint32_t places[]{10000,1000,100,10,1};
        const auto revision=b.machine.StorageRevision();
        for (unsigned digit=0;digit<FirmwareMenu::PuffDigitCount;++digit) {
            const auto desired=target/places[digit]%10;
            for (uint32_t click=0;click<desired;++click) if (!Click(b,true)) return false;
            const auto staged=ObserveRegressionState(b.machine);
            if (!b.Require(staged.menuActive==4 && staged.menuPuffDigit==digit && staged.puffs==initial.puffs &&
                staged.menuPuffValue/places[digit]%10==desired && b.machine.StorageRevision()==revision,
                "digit edit mutated committed state, another digit, or Flash before confirmation") ||
                !Raster(b,source,limit)) return false;
            if (!Fire(b)) return false;
        }
        if (!b.Advance(500000)) return false;
        const auto changed=ObserveRegressionState(b.machine);
        if (!b.Require(changed.menuActive==1 && changed.puffs==target && b.machine.StorageRevision()>revision &&
            Power(changed)==Power(initial) && changed.mode==initial.mode && changed.locked==initial.locked &&
            changed.powerLimit==initial.powerLimit && changed.resistanceMilliOhms==initial.resistanceMilliOhms,
            "fifth Fire failed native persistence or corrupted neighboring settings") ||
            !Exit(b,source.menuPatch)) return false;
        const auto saved=b.machine.SaveStorage();
        Bench reopened;
        if (!reopened.machine.Open(source,code,saved,b.error) || !reopened.Advance(250000) || !reopened.Boot() ||
            !reopened.Advance(800000) ||
            !b.Require(ObserveRegressionState(reopened.machine).puffs==target,
                "changed puff count was not recovered from actual Flash after cold reopen")) return false;
        // Native increment must still use the original halfword/split/word ABI.
        if (!reopened.Hold(true,false,false,500000,700000) ||
            ObserveRegressionState(reopened.machine).puffs!=target+1) {
            b.error="ordinary native puff increment broke after editing the counter; cold machine: "+
                reopened.error+"; "+FirmwareDevice::DescribeRegressionState(reopened.machine);
            return false;
        }
        if (source.edition==FirmwareEdition::Factory && record.linked_base==0x08003000 &&
            !RangeError(reopened,source,limit)) { b.error=reopened.error; return false; }
        // Reset before the fifth Fire discards the staged value.
        if (!OpenEditor(b,source,limit)) return false;
        for (unsigned digit=0;digit+1<FirmwareMenu::PuffDigitCount;++digit) if (!Fire(b)) return false;
        if (!Click(b,true) || !b.Require(ObserveRegressionState(b.machine).puffs==target,
            "partial puff edit changed native RAM") || !b.machine.Reset(b.error) ||
            !b.Advance(250000) || !b.Boot() ||
            !b.Require(ObserveRegressionState(b.machine).puffs==target,
                "reset before confirmation persisted an unfinished puff edit")) return false;
        {
            if (!OpenEditor(b,source,limit)) return false;
            for (unsigned digit=0;digit+1<FirmwareMenu::PuffDigitCount;++digit) if (!Fire(b)) return false;
            if (!Click(b,true)) return false;
            b.inputs.batteryMillivolts=2850;
            b.machine.SetInputs(b.inputs);
            if (!b.Advance(1000000) || !b.Require(ObserveRegressionState(b.machine).puffs==target,
                "low voltage exposed an unconfirmed staged puff count")) return false;
            if (ObserveRegressionState(b.machine).menuActive==4) {
                const auto unsafeRevision=b.machine.StorageRevision();
                if (!Fire(b) || !b.Advance(200000)) return false;
                const auto refused=ObserveRegressionState(b.machine);
                if (!b.Require(refused.menuActive==4 && refused.menuPuffStatus==1 && refused.puffs==target &&
                    b.machine.StorageRevision()==unsafeRevision,
                    "unsafe supply allowed counter programming or falsely reported a successful save") ||
                    !Raster(b,source,limit)) return false;
            }
        }
        return true;
    };
    const bool ok=run();
    if (!ok) error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    return ok;
}
}

inline bool RunDevicePuffEditorRegressions(std::string& error, std::ostream* report=nullptr) {
    unsigned visited=0;
    for (const auto edition:{FirmwareEdition::Score,FirmwareEdition::Factory}) {
        FirmwarePackage package;
        StudioState source;
        if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,edition) ||
            !source.LoadPackage(std::move(package),"native five-digit puff editor",error) ||
            !DevicePuffEditorRegressionDetail::Envelope(source,error)) return false;
        for (const auto& record:source.firmware.records) {
            const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            const bool ok=DevicePuffEditorRegressionDetail::Row(source,record,error);
            if (report) *report<<(edition==FirmwareEdition::Score?"Score":"Factory")<<"/HW "<<std::hex<<code<<std::dec
                <<": "<<(ok?"PASS puff digits, unchanged/no-write, native Flash/reopen, increment, reset and inhibit":error)<<'\n'<<std::flush;
            if (!ok) return false;
            ++visited;
        }
    }
    if (visited!=16) { error="Puff editor did not cover all sixteen native revisions"; return false; }
    if (report) *report<<"NATIVE PUFF EDITOR REGRESSIONS OK records=16\n";
    return true;
}

inline bool RunDevicePuffEditorFactory3148Regressions(std::string& error, std::ostream* report=nullptr) {
    FirmwarePackage package;
    StudioState source;
    if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,FirmwareEdition::Factory) ||
        !source.LoadPackage(std::move(package),"Factory3148 puff editor regression",error)) return false;
    for (const auto& record:source.firmware.records) {
        if (record.device_code[0]!=0x31 || record.device_code[1]!=0x48) continue;
        const bool ok=DevicePuffEditorRegressionDetail::Row(source,record,error);
        if (report) *report<<"FACTORY3148 PUFF EDITOR REGRESSIONS "<<(ok?"OK":"FAILED")<<'\n';
        return ok;
    }
    error="Factory3148 puff editor regression record is absent";
    return false;
}
