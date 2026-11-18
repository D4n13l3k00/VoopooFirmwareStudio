#pragma once
#include "device_puff_editor_regressions.h"
#include "device_achievement_regressions.h"

namespace DeviceMenuHoldRegressionDetail {
using namespace DeviceMenuRegressionDetail;
inline std::vector<uint8_t> Record(const Bench& b, const StudioState& source, const HardwareRecord& record) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const bool low=record.linked_base==0x08003000, history=code==0x3013 || code==0x3014;
    const size_t bytes=source.edition==FirmwareEdition::Score?(low?(history?94:52):88):
        (low?(history?90:46):84);
    std::vector<uint8_t> value(bytes);
    if (!ObserveRegressionState(b.machine,value).mapped) return {};
    return value;
}
inline bool Repeat(Bench& b, uint32_t limit, bool ranks=false, bool reverse=false) {
    const auto before=ObserveRegressionState(b.machine);
    const auto revision=b.machine.StorageRevision();
    const auto storage=b.machine.SaveStorage();
    const auto value=[&] { const auto s=ObserveRegressionState(b.machine); return ranks?s.menuPuffDigit:s.menuPuffValue; };
    const bool plus=!ranks || !reverse;
    b.inputs.plus=plus; b.inputs.minus=!plus; b.machine.SetInputs(b.inputs);
    unsigned changes=0, earlyChanges=0, fastChanges=0;
    uint32_t previous=value();
    for (unsigned elapsed=20000;elapsed<=1500000;elapsed+=20000) {
        if (!NoHeat(b,20000,before.puffs)) return false;
        const auto state=ObserveRegressionState(b.machine);
        const auto next=value();
        if (next!=previous) {
            ++changes;
            if (elapsed<350000) ++earlyChanges;
            if (elapsed>=1000000) ++fastChanges;
            if (!b.Require(ranks?next==(previous+1)%FirmwareMenu::RankRowCount:
                next/10==before.menuPuffValue/10 && next%10==(previous%10+1)%10,
                "held navigation jumped a rank, carried a digit or skipped a repeat")) return false;
        }
        if (!b.Require(state.menuActive==before.menuActive && (ranks || state.menuPuffDigit==4) &&
            (ranks || next<=limit) && state.puffs==before.puffs &&
            state.scoreRawTotal==before.scoreRawTotal && state.scoreBestSinglePuff==before.scoreBestSinglePuff &&
            Power(state)==Power(before) && b.machine.StorageRevision()==revision,
            "held navigation changed committed settings, selected digit, bound, heating or Flash")) return false;
        previous=next;
    }
    if (!b.Require(earlyChanges==1 && changes>=10 && fastChanges>=6,
        "held navigation lacks its immediate step, repeat delay or accelerated interval")) return false;
    b.inputs.plus=b.inputs.minus=false; b.machine.SetInputs(b.inputs);
    if (!b.Advance(300000) || !b.Require(value()==previous,
        "native release/click summary advanced a held edit twice")) return false;
    b.inputs.plus=b.inputs.minus=true; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,450000,before.puffs) || !b.Require(value()==previous,
        "both buttons jumped the editor or invoked stock history")) return false;
    b.inputs.minus=false; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,450000,before.puffs) || !b.Require(value()==previous,
        "mixed-key release restarted a held edit before all keys were released")) return false;
    b.inputs.plus=false; b.machine.SetInputs(b.inputs);
    if (!b.Advance(300000) || !b.Require(value()==previous,
        "mixed-key release summary advanced the editor")) return false;
    b.inputs.plus=true; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,120000,before.puffs)) return false;
    const auto mixedValue=value();
    b.inputs.fire=true; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,850000,before.puffs) || !b.Require(value()==mixedValue &&
        ObserveRegressionState(b.machine).menuActive==before.menuActive,
        "Fire plus a held key confirmed/canceled a digit or retained its repeat timer")) return false;
    b.inputs.fire=false; b.machine.SetInputs(b.inputs);
    if (!NoHeat(b,400000,before.puffs) || !b.Require(value()==mixedValue,
        "Fire chord release restarted navigation before all keys released")) return false;
    b.inputs.plus=false; b.machine.SetInputs(b.inputs);
    return b.Advance(300000) && b.Require(value()==mixedValue && b.machine.StorageRevision()==revision &&
        b.machine.SaveStorage()==storage,"held editor changed Flash or advanced on chord release");
}
inline bool Row(StudioState& source, const HardwareRecord& record, std::string& error) {
    const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
    const auto limit=FirmwareMenu::PuffLimit(record);
    source.menuPatch={true,false};
    source.menuPatch.enabledItems=FirmwareMenu::PuffEditorMask;
    source.menuPatch.infoFields=0;
    source.menuPatch.reverseNavigation=(code&1)!=0;
    const bool achievements=source.edition==FirmwareEdition::Score && FirmwareMenu::AchievementSupported(record);
    if (achievements) source.menuPatch.enabledItems|=FirmwareMenu::ScoreEditorMask|FirmwareMenu::RankSelectorMask;
    Bench b;
    const auto run=[&] {
        if (!b.machine.Open(source,code,{},b.error) || !b.Advance(250000) || !b.Boot() || !b.Advance(800000) ||
            !DevicePuffEditorRegressionDetail::OpenEditor(b,source,limit)) return false;
        const auto baseline=Record(b,source,record);
        if (!b.Require(!baseline.empty(), "native settings snapshot is unavailable for this revision")) return false;
        const auto initial=ObserveRegressionState(b.machine);
        // Exercise the physical 9,999 bound as well as the five-digit bound.
        b.inputs.plus=true; b.machine.SetInputs(b.inputs);
        for (unsigned elapsed=0;elapsed<1200000;elapsed+=20000) {
            if (!NoHeat(b,20000,initial.puffs)) return false;
            const auto held=ObserveRegressionState(b.machine);
            if (!b.Require(held.menuPuffDigit==0 && held.menuPuffValue<=limit &&
                held.menuPuffValue%10000==initial.menuPuffValue%10000,
                "held leading digit carried or exceeded the revision limit")) return false;
        }
        b.inputs.plus=false; b.machine.SetInputs(b.inputs);
        if (!b.Advance(300000)) return false;
        for (unsigned digit=0;digit<4;++digit) if (!Fire(b)) return false;
        if (!Repeat(b,limit) || !DevicePuffEditorRegressionDetail::Raster(b,source,limit) || !CancelHeldFire(b) ||
            !b.Require(Record(b,source,record)==baseline,
                "puff cancel changed native settings bytes or CRC") ||
            !DevicePuffEditorRegressionDetail::OpenEditor(b,source,limit) ||
            !DevicePuffEditorRegressionDetail::FinishUnchanged(b,initial.puffs)) return false;
        if (achievements) {
            if (!DeviceAchievementRegressionDetail::OpenScore(b,source)) return false;
            for (unsigned digit=0;digit<4;++digit) if (!Fire(b)) return false;
            if (!Repeat(b,99999) || !DeviceAchievementRegressionDetail::Raster(b,source) || !CancelHeldFire(b) ||
                !b.Require(Record(b,source,record)==baseline,
                    "points cancel changed native settings bytes or CRC") ||
                !DeviceAchievementRegressionDetail::OpenScore(b,source) ||
                !DeviceAchievementRegressionDetail::Stage(b,source,initial.scoreRawTotal/1000) ||
                !DeviceAchievementRegressionDetail::OpenRank(b,source) ||
                !Repeat(b,0,true,source.menuPatch.reverseNavigation) ||
                !DeviceAchievementRegressionDetail::Raster(b,source,true) || !CancelHeldFire(b) ||
                !b.Require(Record(b,source,record)==baseline,
                    "rank cancel changed native settings bytes or CRC")) return false;
        }
        // A partial edit and its held repeat state must not survive reset.
        if (!DevicePuffEditorRegressionDetail::OpenEditor(b,source,limit)) return false;
        for (unsigned digit=0;digit<4;++digit) if (!Fire(b)) return false;
        if (!Click(b,true) || !b.machine.Reset(b.error) || !b.Advance(250000) || !b.Boot()) return false;
        const auto reset=ObserveRegressionState(b.machine);
        return b.Require(reset.puffs==initial.puffs && reset.scoreRawTotal==initial.scoreRawTotal && !reset.menuActive,
            "reset retained a staged digit, repeat state or native editor value");
    };
    const bool ok=run();
    if (!ok) error=b.error+"; "+FirmwareDevice::DescribeRegressionState(b.machine);
    return ok;
}
}
inline bool RunDeviceMenuHoldRegressions(std::string& error, std::ostream* report=nullptr) {
    unsigned puffs=0, achievements=0;
    for (const auto edition:{FirmwareEdition::Score,FirmwareEdition::Factory}) {
        FirmwarePackage package;
        StudioState source;
        if (!LoadEmbeddedFirmware(package,error,FirmwareModel::DragS,edition) ||
            !source.LoadPackage(std::move(package),"native editor hold controls",error)) return false;
        for (const auto& record:source.firmware.records) {
            const bool ok=DeviceMenuHoldRegressionDetail::Row(source,record,error);
            const uint16_t code=uint16_t(record.device_code[0])<<8|record.device_code[1];
            if (report) *report<<(edition==FirmwareEdition::Score?"Score":"Factory")<<"/HW "<<std::hex<<code<<std::dec
                <<": "<<(ok?"PASS held Fire discard, accelerated digits/ranks, mixed keys, release, CRC/Flash/reset":error)
                <<'\n'<<std::flush;
            if (!ok) return false;
            ++puffs;
            if (edition==FirmwareEdition::Score && FirmwareMenu::AchievementSupported(record)) ++achievements;
        }
    }
    if (puffs!=16 || achievements!=7) { error="hold matrix omitted a puff/achievement revision"; return false; }
    return true;
}
