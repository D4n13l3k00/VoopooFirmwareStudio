#pragma once
#include "device_puff_editor_regressions.h"

namespace DeviceAchievementRegressionDetail {
using namespace DeviceMenuRegressionDetail;
inline unsigned Rank(uint32_t raw) {
    unsigned rank=0;
    while (rank+1<FirmwareMenu::RankCount && raw/1000>=FirmwareMenu::RankPoints[rank+1]) ++rank;
    return rank;
}
inline bool Raster(Bench& b, const StudioState& source, bool ranks=false) {
    const auto native=ObserveRegressionState(b.machine);
    FirmwareMenu::PreviewState state;
    state.selection=native.menuSelection;
    state.editingScore=!ranks;
    state.selectingRank=ranks;
    state.rankSelection=native.menuPuffDigit;
    state.puffDigit=native.menuPuffDigit;
    state.puffValue=native.menuPuffValue;
    state.originalPuffs=native.menuPuffOriginal;
    state.puffStatus=native.menuPuffStatus;
    FirmwareMenu::PreviewValues values;
    values.scorePoints=native.scoreRawTotal/1000;
    std::array<uint32_t,80*160> expected;
    const auto& actual=b.machine.Current().display.rgba;
    if (!b.Require(FirmwareMenu::RenderPreview(source.menuPatch,state,values,expected) &&
        actual.size()==expected.size(),"native achievement LCD geometry is invalid")) return false;
    const auto mismatch=std::mismatch(expected.begin(),expected.end(),actual.begin());
    const auto pixel=size_t(mismatch.first-expected.begin());
    if (pixel==expected.size()) return true;
    if (const char* prefix=std::getenv("VFS_MENU_CAPTURE");prefix && *prefix) {
        std::string captureError;
        FirmwareBitmap::SavePngRgb565(std::string(prefix)+"-achievement-actual.png",
            actual.data(),80,160,captureError);
        FirmwareBitmap::SavePngRgb565(std::string(prefix)+"-achievement-expected.png",
            expected.data(),80,160,captureError);
    }
    const auto message="native achievement LCD differs at pixel "+
        std::to_string(pixel%80)+","+std::to_string(pixel/80);
    return b.Require(false,message.c_str());
}
inline bool OpenScore(Bench& b, const StudioState& source) {
    if (!ObserveRegressionState(b.machine).menuActive && !Enter(b)) return false;
    const auto before=ObserveRegressionState(b.machine);
    return Select(b,source.menuPatch,FirmwareMenu::Item::ScoreEditor) &&
        b.Require(ObserveRegressionState(b.machine).menuActive==5 &&
            ObserveRegressionState(b.machine).menuPuffDigit==0 &&
            ObserveRegressionState(b.machine).menuPuffValue==before.scoreRawTotal/1000 &&
            ObserveRegressionState(b.machine).menuPuffOriginal==before.scoreRawTotal,
            "POINTS did not load the actual total with the first digit immediately selected") && Raster(b,source);
}
inline bool Stage(Bench& b, const StudioState& source, uint32_t points, bool confirm=true) {
    constexpr uint32_t places[]{10000,1000,100,10,1};
    const auto before=ObserveRegressionState(b.machine);
    const auto revision=b.machine.StorageRevision();
    for (unsigned digit=0;digit<5;++digit) {
        const auto desired=points/places[digit]%10;
        for (unsigned clicks=0;clicks<10 &&
            ObserveRegressionState(b.machine).menuPuffValue/places[digit]%10!=desired;++clicks)
            if (!Click(b,true)) return false;
        const auto native=ObserveRegressionState(b.machine);
        if (!b.Require(native.menuPuffDigit==digit && native.menuPuffValue/places[digit]%10==desired &&
            native.scoreRawTotal==before.scoreRawTotal && native.scoreBestSinglePuff==before.scoreBestSinglePuff &&
            native.puffs==before.puffs && Power(native)==Power(before) && native.pwmStarts==before.pwmStarts &&
            !native.pwmCompare && b.machine.StorageRevision()==revision,
            "unconfirmed POINTS digit changed native points, best, puffs, power or Flash") || !Raster(b,source)) return false;
        if ((digit<4 || confirm) && !Fire(b)) return false;
    }
    return true;
}
inline bool OpenRank(Bench& b, const StudioState& source) {
    if (!ObserveRegressionState(b.machine).menuActive && !Enter(b)) return false;
    const auto before=ObserveRegressionState(b.machine);
    return Select(b,source.menuPatch,FirmwareMenu::Item::RankSelector) &&
        b.Require(ObserveRegressionState(b.machine).menuActive==6 &&
            ObserveRegressionState(b.machine).menuPuffDigit==Rank(before.scoreRawTotal),
            "SET RANK did not initially select the actual inclusive native rank") && Raster(b,source,true);
}
inline bool ChooseRank(Bench& b, const StudioState& source, unsigned rank) {
    for (unsigned clicks=0;clicks<FirmwareMenu::RankRowCount && ObserveRegressionState(b.machine).menuPuffDigit!=rank;++clicks)
        if (!Click(b,!source.menuPatch.reverseNavigation)) return false;
    return b.Require(ObserveRegressionState(b.machine).menuPuffDigit==rank,
        "rank navigation did not follow configured physical direction") && Raster(b,source,true);
}
inline bool NativeRankLcd(Bench& b, const HardwareRecord& record, unsigned rank) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const uint32_t table=code==0x3013?0x08010434u:code==0x3014?0x08010444u:
        code==0x3029?0x0800f434u:code==0x302a?0x0800f444u:
        code==0x314d?0x08010588u:code==0x3103?0x08010604u:0x08010748u;
    const size_t descriptor=table-record.linked_base+rank*12;
    const auto& pixels=b.machine.Current().display.rgba;
    if (!b.Require(descriptor+12<=record.image.size() && pixels.size()==80*160,
        "native rank descriptor or physical LCD is unavailable")) return false;
    for (unsigned part=0;part<2;++part) {
        const uint32_t width=part?47u:24u, height=part?7u:24u;
        const size_t address=Word(record.image.data()+descriptor+part*4)-record.linked_base;
        const size_t bytes=((width+7)/8)*height;
        std::vector<uint8_t> gray;
        if (!b.Require(address<=record.image.size() && bytes<=record.image.size()-address &&
            FirmwareBitmap::DecodeMask(record.image.data()+address,bytes,width,height,gray),
            "native rank resource cannot be decoded")) return false;
        for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x)
            if (!b.Require((gray[y*width+x]!=0)==
                (pixels[(y+(part?132:128))*80+x+(part?31:4)]!=0xff000000u),
                "native Main rank icon/label does not derive from the persisted points threshold")) return false;
    }
    return true;
}
// Decode actual native serialized records, not injected guest settings. Compare
// every persistent byte except total, CRC and native-refreshed RTC fields.
inline std::vector<uint8_t> Record(const Bench& b, const HardwareRecord& record, uint32_t raw) {
    const bool low=record.linked_base==0x08003000;
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const size_t bytes=low?((code==0x3013 || code==0x3014)?94:52):88;
    const auto storage=b.machine.SaveStorage();
    const auto current=ObserveRegressionState(b.machine);
    for (size_t slot:low?std::array<size_t,2>{0x800,0xc00}:std::array<size_t,2>{0,0x800}) {
        if (storage.size()<24+slot+bytes) return {};
        std::vector<uint8_t> value(storage.begin()+24+slot,storage.begin()+24+slot+bytes);
        if (!low) for (size_t at=0;at<bytes;at+=4) std::reverse(value.begin()+at,value.begin()+at+4);
        const auto u16=[&](size_t at) { return uint32_t(value[at])|uint32_t(value[at+1])<<8; };
        const auto total=low?(u16(0xc)<<16|u16(0xe)):Word(value.data()+0x18);
        const auto best=low?u16(0xa):Word(value.data()+0x14);
        const auto puffs=low?((code==0x3013 || code==0x3014)?u16(0x50):
            (u16(0x2e)<<16)|u16(0x30)):Word(value.data()+0x50);
        // The other native bank can still contain the initial zero-score record.
        if (total==raw && best==current.scoreBestSinglePuff && puffs==current.puffs) return value;
    }
    return {};
}
inline std::vector<uint8_t> CurrentRecord(const Bench& b, const HardwareRecord& record) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const size_t bytes=record.linked_base==0x08003000?
        ((code==0x3013 || code==0x3014)?94:52):88;
    std::vector<uint8_t> value(bytes);
    // Boot can reset a live session counter before the next native Flash save.
    if (!ObserveRegressionState(b.machine,value).mapped) return {};
    return value;
}
inline bool PreservedRecord(Bench& b, const HardwareRecord& record,
    const std::vector<uint8_t>& before, uint32_t raw) {
    const auto after=Record(b,record,raw);
    const bool low=record.linked_base==0x08003000;
    const size_t total=low?0xc:0x18, crc=low?after.size()-2:84;
    if (!b.Require(!before.empty() && after.size()==before.size(),
        "native transaction did not produce a complete recoverable settings record")) return false;
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    for (size_t at=0;at<crc;++at) {
        // History saves refresh their five RTC halfwords through the stock transaction.
        if (low && (code==0x3013 || code==0x3014) && at>=0x52 && at<0x5c) continue;
        if (at>=total && at<total+4) continue;
        if (before[at]!=after[at]) {
            b.error="achievement settings byte "+std::to_string(at)+" changed "+
                std::to_string(before[at])+" -> "+std::to_string(after[at])+" outside points/CRC/RTC";
            return false;
        }
    }
    return true;
}
inline bool Oversized(Bench& owner, const StudioState& source, uint16_t code,
    const std::vector<uint8_t>& storage) {
    Bench b;
    if (!b.machine.Open(source,code,storage,b.error) || !b.Advance(250000) || !b.Boot() ||
        !b.Advance(800000)) {
        owner.error=b.error; return false;
    }
    for (unsigned puff=0;puff<4 && ObserveRegressionState(b.machine).scoreRawTotal/1000<=99999;++puff) {
        const auto before=ObserveRegressionState(b.machine);
        if (!b.Hold(true,false,false,1000000,1500000)) { owner.error=b.error; return false; }
        const auto after=ObserveRegressionState(b.machine);
        if (!b.Require(after.puffs==before.puffs+1 && after.scoreRawTotal>before.scoreRawTotal,
            "real puff did not accumulate the native oversized-points fixture")) {
            owner.error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine); return false;
        }
    }
    if (!b.Require(ObserveRegressionState(b.machine).scoreRawTotal/1000>99999,
        "native puffs did not cross the five-digit points boundary") || !OpenScore(b,source)) {
        owner.error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine); return false;
    }
    const auto original=ObserveRegressionState(b.machine);
    const auto revision=b.machine.StorageRevision();
    const bool ok=b.Require(original.scoreRawTotal/1000>99999 && original.menuPuffStatus==2,
        "native oversized points were truncated instead of presenting COUNT TOO BIG") &&
        b.Hold(false,true,false,1200000,300000) && b.Hold(false,true,true,450000,300000) &&
        Click(b,true) && Click(b,false) && Raster(b,source) && Fire(b) &&
        b.Require(ObserveRegressionState(b.machine).menuActive==1 &&
            ObserveRegressionState(b.machine).scoreRawTotal==original.scoreRawTotal &&
            b.machine.StorageRevision()==revision,
            "oversized native POINTS back/edit attempted truncation or persistence");
    if (!ok) owner.error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    return ok;
}
inline bool UnsafeRank(Bench& owner, const StudioState& source, uint16_t code,
    const std::vector<uint8_t>& storage) {
    Bench b;
    if (!b.machine.Open(source,code,storage,b.error) || !b.Advance(250000) || !b.Boot() ||
        !b.Advance(800000) || !OpenRank(b,source) || !ChooseRank(b,source,0)) {
        owner.error=b.error; return false;
    }
    const auto original=ObserveRegressionState(b.machine);
    b.inputs.batteryMillivolts=2850; b.machine.SetInputs(b.inputs);
    if (!b.Advance(1000000)) { owner.error=b.error; return false; }
    const auto revision=b.machine.StorageRevision();
    const bool ok=Fire(b) && b.Advance(300000) &&
        b.Require(ObserveRegressionState(b.machine).scoreRawTotal==original.scoreRawTotal &&
            ObserveRegressionState(b.machine).scoreBestSinglePuff==original.scoreBestSinglePuff &&
            b.machine.StorageRevision()==revision,
            "unsafe supply allowed SET RANK to program native settings");
    if (!ok) owner.error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    return ok;
}
inline bool Row(StudioState& source, const HardwareRecord& record, std::string& error) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    source.menuPatch={true,false};
    source.menuPatch.enabledItems=code==0x314d || code==0x3014?24u:
        code==0x3105 || code==0x3029?28u:FirmwareMenu::FunctionalMask;
    source.menuPatch.infoFields=(source.menuPatch.enabledItems&FirmwareMenu::InformationMask)?
        FirmwareMenu::AllInfoFields:0;
    // Both physical navigation conventions are covered across each family.
    source.menuPatch.reverseNavigation=(code&1)!=0;
    Bench b;
    const auto run=[&]() {
        if (!b.machine.Open(source,code,{},b.error) || !b.Advance(250000) || !b.Boot() || !b.Advance(800000)) return false;
        // Obtain a nonzero best SINGLE-PUFF and fractional total by real heating.
        for (unsigned puff=0;puff<6 && (!ObserveRegressionState(b.machine).scoreBestSinglePuff ||
            !ObserveRegressionState(b.machine).scoreRawTotal || ObserveRegressionState(b.machine).scoreRawTotal%1000==0);++puff)
            if (!b.Hold(true,false,false,530000+puff*70000,1500000)) return false;
        const auto initial=ObserveRegressionState(b.machine);
        if (!b.Require(initial.scoreBestSinglePuff>0 && initial.scoreRawTotal%1000!=0,
            "physical puffs did not establish the nonzero best/fractional native score fixture") || !OpenScore(b,source)) return false;
        const auto revision=b.machine.StorageRevision();
        if (!Stage(b,source,initial.scoreRawTotal/1000) ||
            !b.Require(ObserveRegressionState(b.machine).menuActive==1 && b.machine.StorageRevision()==revision,
                "unchanged POINTS confirmation unnecessarily wrote Flash")) return false;
        if (!OpenScore(b,source)) return false;
        if (!Stage(b,source,12345,false) || !CancelHeldFire(b) || !OpenScore(b,source)) return false;
        b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
        if (!NoHeat(b,300000,initial.puffs)) return false;
        b.inputs.airflow=false; b.machine.SetInputs(b.inputs);
        if (!Stage(b,source,12345) || !b.Advance(300000)) return false;
        const uint32_t first=12345000+initial.scoreRawTotal%1000;
        auto changed=ObserveRegressionState(b.machine);
        if (!b.Require(changed.scoreRawTotal==first && changed.scoreBestSinglePuff==initial.scoreBestSinglePuff &&
            changed.puffs==initial.puffs && Power(changed)==Power(initial) && changed.menuActive==1 &&
            b.machine.StorageRevision()>revision,
            "fifth Fire did not preserve native fractional units/best/puffs/power and commit POINTS")) return false;
        const auto firstRecord=CurrentRecord(b,record);
        if (!OpenScore(b,source) || !Stage(b,source,99999) || !b.Advance(300000) ||
            !PreservedRecord(b,record,firstRecord,99999000+initial.scoreRawTotal%1000)) return false;
        const auto saved=b.machine.SaveStorage();
        Bench cold;
        if (!cold.machine.Open(source,code,saved,b.error) || !cold.Advance(250000) || !cold.Boot() || !cold.Advance(800000)) {
            b.error=cold.error; return false;
        }
        const auto recovered=ObserveRegressionState(cold.machine);
        if (!b.Require(recovered.scoreRawTotal==99999000+initial.scoreRawTotal%1000 &&
            recovered.scoreBestSinglePuff==initial.scoreBestSinglePuff && recovered.puffs==initial.puffs &&
            Power(recovered)==Power(initial),"cold boot did not load the native score record exactly")) return false;
        if (record.linked_base!=0x08003000 && !Oversized(b,source,code,saved)) return false;
        // BACK is a real native row: never rewrite points, best or Flash.
        if (!OpenRank(b,source) || !ChooseRank(b,source,FirmwareMenu::RankCount)) return false;
        if (!ChooseRank(b,source,0) || !CancelHeldFire(b) || !OpenRank(b,source) ||
            !ChooseRank(b,source,FirmwareMenu::RankCount)) return false;
        const auto backBefore=ObserveRegressionState(b.machine);
        const auto backRevision=b.machine.StorageRevision();
        const auto backStorage=b.machine.SaveStorage();
        if (!Fire(b) || !b.Require(ObserveRegressionState(b.machine).menuActive==1 &&
            ObserveRegressionState(b.machine).scoreRawTotal==backBefore.scoreRawTotal &&
            ObserveRegressionState(b.machine).scoreBestSinglePuff==backBefore.scoreBestSinglePuff &&
            ObserveRegressionState(b.machine).puffs==backBefore.puffs &&
            Power(ObserveRegressionState(b.machine))==Power(backBefore) &&
            b.machine.StorageRevision()==backRevision && b.machine.SaveStorage()==backStorage,
            "native rank BACK changed settings, wrote Flash or failed to return to menu")) return false;
        b.machine=std::move(cold.machine); b.inputs={};
        // Every native threshold, including lowering KING to IRON, and every row.
        for (unsigned rank=0;rank<FirmwareMenu::RankCount;++rank) {
            if (!OpenRank(b,source)) return false;
            const auto before=ObserveRegressionState(b.machine);
            const auto beforeRecord=CurrentRecord(b,record);
            const unsigned selected=before.menuPuffDigit;
            if (!Click(b,source.menuPatch.reverseNavigation) ||
                !b.Require(ObserveRegressionState(b.machine).menuPuffDigit==
                    (selected+FirmwareMenu::RankRowCount-1)%FirmwareMenu::RankRowCount,
                    "configured rank UP did not wrap to the previous row") || !Raster(b,source,true) ||
                !ChooseRank(b,source,rank)) return false;
            b.inputs.airflow=true; b.machine.SetInputs(b.inputs);
            if (!NoHeat(b,200000,initial.puffs)) return false;
            b.inputs.airflow=false;
            b.inputs.fire=true; b.machine.SetInputs(b.inputs);
            if (!NoHeat(b,400000,initial.puffs)) return false;
            b.inputs.fire=false; b.machine.SetInputs(b.inputs);
            if (!b.Advance(1000000)) return false;
            const auto committed=ObserveRegressionState(b.machine);
            if (!b.Require(!committed.menuActive && committed.scoreRawTotal==FirmwareMenu::RankPoints[rank]*1000 &&
                committed.scoreBestSinglePuff==initial.scoreBestSinglePuff && committed.puffs==initial.puffs &&
                Power(committed)==Power(initial),"rank confirmation failed native persistence/Main release guard or corrupted settings") ||
                !NativeRankLcd(b,record,rank) ||
                !PreservedRecord(b,record,beforeRecord,FirmwareMenu::RankPoints[rank]*1000)) return false;
            const auto rankRevision=b.machine.StorageRevision();
            if (!OpenRank(b,source) || !b.Require(ObserveRegressionState(b.machine).menuPuffDigit==rank,
                "exact rank minimum selected the wrong inclusive native rank") || !Fire(b) || !b.Advance(800000) ||
                !b.Require(b.machine.StorageRevision()==rankRevision,
                    "unchanged rank minimum consumed another native Flash transaction")) return false;
        }
        if (!UnsafeRank(b,source,code,b.machine.SaveStorage())) return false;
        // An unfinished numeric edit is not persistent across reset.
        const auto last=ObserveRegressionState(b.machine).scoreRawTotal;
        if (!OpenScore(b,source) || !Stage(b,source,10000,false) || !b.machine.Reset(b.error) ||
            !b.Advance(250000) || !b.Boot() || !b.Advance(800000) ||
            !b.Require(ObserveRegressionState(b.machine).scoreRawTotal==last,
                "reset persisted an unfinished native POINTS edit")) return false;
        if (!OpenScore(b,source) || !Stage(b,source,10000,false)) return false;
        b.inputs.batteryMillivolts=2850; b.machine.SetInputs(b.inputs);
        if (!b.Advance(1000000)) return false;
        const auto unsafeRevision=b.machine.StorageRevision();
        if (!Fire(b) || !b.Advance(300000) || !b.Require(ObserveRegressionState(b.machine).scoreRawTotal==last &&
            b.machine.StorageRevision()==unsafeRevision,"unsafe supply allowed achievement Flash programming")) return false;
        return true;
    };
    const bool ok=run();
    if (!ok) error=b.error+"; HW "+std::to_string(code)+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    return ok;
}
inline bool FactoryRow(StudioState& source, const HardwareRecord& record, std::string& error) {
    source.menuPatch={true,false};
    source.menuPatch.enabledItems=FirmwareMenu::ScoreEditorMask|FirmwareMenu::RankSelectorMask;
    source.menuPatch.infoFields=0;
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    Bench b;
    if (!b.machine.Open(source,code,{},b.error) || !b.Advance(250000) || !b.Boot() ||
        !b.Advance(800000) || !Enter(b)) { error=b.error; return false; }
    const auto before=ObserveRegressionState(b.machine);
    if (!Fire(b) || !b.Advance(800000) || !b.Require(!ObserveRegressionState(b.machine).menuActive &&
        ObserveRegressionState(b.machine).pwmStarts==before.pwmStarts &&
        ObserveRegressionState(b.machine).puffs==before.puffs,
        "Factory emitted an achievement editor instead of its only effective EXIT row")) {
        error=b.error; return false;
    }
    return true;
}
inline bool Envelope(StudioState& source, std::string& error) {
    FirmwareMenu::Settings permutation{true,false};
    std::sort(permutation.order.begin(),permutation.order.end());
    unsigned orders=0;
    do {
        if (!FirmwareMenu::ValidSettings(permutation)) { error="six-item native permutation is invalid"; return false; }
        ++orders;
    } while (std::next_permutation(permutation.order.begin(),permutation.order.end()));
    if (orders!=720) { error="six-item shell did not cover all 720 permutations"; return false; }
    std::vector<uint8_t> inner;
    if (!source.BuildZwdfBytes(inner,error)) return false;
    FirmwarePackage original;
    if (!FirmwarePackage::Parse(inner,original,error)) return false;
    constexpr uint32_t variants[]{8,16,24,12,20,28,31,9,18};
    for (unsigned variant=0;variant<std::size(variants);++variant) {
        FirmwareMenu::Settings settings{true,false};
        settings.enabledItems=variants[variant];
        settings.infoFields=(variant&1)?1:7;
        std::rotate(settings.order.begin(),settings.order.begin()+variant%6,settings.order.end());
        auto bytes=inner;
        BuildReport report;
        FirmwarePackage package;
        if (!FirmwareMenu::Apply(bytes,settings,report,error) || !FirmwarePackage::Parse(bytes,package,error)) return false;
        for (size_t i=0;i<package.records.size();++i) {
            const auto& record=package.records[i];
            const auto costs=FirmwareMenu::FlashCosts(original.records[i],settings);
            const bool supported=FirmwareMenu::AchievementSupported(original.records[i]);
            const size_t start=Word(record.image.data()+record.image.size()-16);
            if (supported!=(source.edition==FirmwareEdition::Score) ||
                costs.common+costs.information+costs.voltage+costs.puffEditor+costs.scoreEditor+costs.rankSelector!=costs.total ||
                costs.total!=record.image.size()-original.records[i].image.size() ||
                costs.total!=FirmwareMenu::FlashBytes(original.records[i],settings) ||
                (!supported && (costs.scoreEditor || costs.rankSelector)) ||
                Word(record.image.data()+start+8)!=1 || Word(record.image.data()+record.image.size()-4)!=1) {
                error="achievement support, public v1 envelope or exact additive Flash cost differs"; return false;
            }
            uint32_t expected=1|(settings.reverseNavigation?4u:0u)|(settings.enabledItems<<3)|(settings.infoFields<<26);
            for (unsigned item=0;item<6;++item) expected|=uint32_t(settings.order[item])<<(8+item*3);
            if (Word(record.image.data()+start+24)!=expected) { error="native public v1 flags are not the clean six-item contract"; return false; }
            if (!supported) for (const char* label:{"POINTS","SET RANK","PLATINUM"}) {
                if (std::search(record.image.begin()+start,record.image.end(),label,label+std::strlen(label)+1)!=record.image.end()) {
                    error="Factory extension contains an unsupported achievement module"; return false;
                }
            }
        }
        FirmwareMenu::Settings restored;
        if (!FirmwareMenu::Extract(package,restored,error) || restored!=settings) return false;
        for (size_t i=0;i<package.records.size();++i) if (package.records[i].image!=original.records[i].image) {
            error="achievement envelope failed exact native inner-image restoration"; return false;
        }
    }
    if (source.edition==FirmwareEdition::Score) {
        // Reject changed native total loads/stores on every hardware revision,
        // even when its general menu hooks remain perfectly stock.
        for (const auto& record:original.records) {
            FirmwarePackage foreign=original;
            foreign.records={record};
            const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            const uint32_t store=record.linked_base!=0x08003000?0x080082f6u:
                (code==0x3013 || code==0x3014)?0x08005dbeu:0x08005bc2u;
            foreign.records[0].image[store-record.linked_base]^=1;
            std::vector<uint8_t> bytes;
            BuildReport packed;
            if (!foreign.PackWith({}, {}, bytes,packed,error)) return false;
            const auto unchanged=bytes;
            FirmwareMenu::Settings settings{true,false};
            settings.enabledItems=FirmwareMenu::ScoreEditorMask|FirmwareMenu::RankSelectorMask;
            std::string rejected;
            if (FirmwareMenu::Apply(bytes,settings,packed,rejected) || bytes!=unchanged) {
                error="achievement editor accepted a foreign native score-state closure"; return false;
            }
        }
    }
    return true;
}
}

inline bool RunDeviceAchievementRegressions(std::string& error, std::ostream* report=nullptr) {
    unsigned visited=0;
    for (const auto edition:{FirmwareEdition::Score,FirmwareEdition::Factory}) {
        FirmwarePackage package;
        StudioState source;
        if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,edition) ||
            !source.LoadPackage(std::move(package),"native points/rank editor",error) ||
            !DeviceAchievementRegressionDetail::Envelope(source,error)) return false;
        if (edition==FirmwareEdition::Factory) {
            for (const auto& record:source.firmware.records)
                if (!DeviceAchievementRegressionDetail::FactoryRow(source,record,error)) return false;
            continue;
        }
        for (const auto& record:source.firmware.records) {
            const auto code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            const bool ok=DeviceAchievementRegressionDetail::Row(source,record,error);
            if (report) *report<<"Score/HW "<<std::hex<<code<<std::dec<<": "<<(ok?
                "PASS native points/remainder/best/Flash/cold boot, all eight rank LCDs, input inhibit and unsafe supply":error)<<'\n'<<std::flush;
            if (!ok) return false;
            ++visited;
        }
    }
    if (visited!=7) { error="achievement regressions did not cover all seven Score revisions"; return false; }
    if (report) *report<<"NATIVE ACHIEVEMENT REGRESSIONS OK records=7 ranks=56 Factory=omitted\n";
    return true;
}

inline bool RunDeviceAchievementRevisionRegressions(std::string& error, uint16_t code, std::ostream* report=nullptr) {
    FirmwarePackage package;
    StudioState source;
    if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,FirmwareEdition::Score) ||
        !source.LoadPackage(std::move(package),"native achievement LCD boundary",error)) return false;
    const auto record=std::find_if(source.firmware.records.begin(),source.firmware.records.end(),
        [code](const auto& candidate) { return (uint16_t(candidate.device_code[0])<<8|candidate.device_code[1])==code; });
    if (record==source.firmware.records.end()) { error="Missing Score hardware revision"; return false; }
    if (!DeviceAchievementRegressionDetail::Row(source,*record,error)) return false;
    if (report) *report<<"SCORE ACHIEVEMENT REGRESSIONS OK HW "<<std::hex<<code<<std::dec<<'\n'<<std::flush;
    return true;
}
