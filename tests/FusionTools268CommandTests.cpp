#include "../Lawn/Fusion/FusionWire.h"
#include "../Lawn/Net/NetCommon.h"
#include <cstdio>
int RunFusionTools268CommandTests(int& checks)
{
    using namespace LawnFusion;using LawnNet::NetBuffer;int failures=0;checks=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionTools268CommandTests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    for(auto action:{FusionCommandKind::EquipmentPickup,FusionCommandKind::EquipmentUse,FusionCommandKind::EquipmentCancel,FusionCommandKind::CannonAim})
    {
        FusionCommand command;command.epoch=3;command.inputEpoch=7;command.sequence=11;command.action=action;
        command.sourceId=29;command.sourceToken=0x12340005;
        if(action==FusionCommandKind::EquipmentUse){command.column=9;command.row=5;}
        if(action==FusionCommandKind::CannonAim){command.boardTargetX=701;command.boardTargetY=542;}
        NetBuffer encoded;CHECK(EncodeFusionCommand(command,encoded));
        CHECK(encoded.Size()==size_t(action==FusionCommandKind::CannonAim?29:action==FusionCommandKind::EquipmentUse?27:25));
        FusionCommand decoded;auto input=encoded;
        CHECK(DecodeFusionCommand(input,decoded)&&!input.Remaining()&&decoded.action==action&&decoded.sourceId==29&&decoded.sourceToken==0x12340005);
        CHECK(decoded.boardTargetX==command.boardTargetX&&decoded.boardTargetY==command.boardTargetY&&decoded.column==command.column&&decoded.row==command.row);
        NetBuffer round;CHECK(EncodeFusionCommand(decoded,round)&&round.mData==encoded.mData);
        for(size_t size=0;size<encoded.Size();++size)
        {
            NetBuffer truncated;truncated.mData.assign(encoded.mData.begin(),encoded.mData.begin()+size);
            FusionCommand untouched;untouched.sequence=987;untouched.boardTargetX=123;
            CHECK(!DecodeFusionCommand(truncated,untouched)&&truncated.mReadPos==0&&untouched.sequence==987&&untouched.boardTargetX==123);
        }
        auto extra=encoded;extra.WriteU8(0);CHECK(!DecodeFusionCommand(extra,decoded)&&extra.mReadPos==0);
        for(int field=0;field<8;++field)
        {
            auto bad=command;
            if(field==0)bad.packetId=1;if(field==1)bad.kind=3;if(field==2)bad.enabled=true;if(field==3)bad.isExtra=true;
            if(field==4)bad.targetId=1;if(field==5)bad.targetToken=1;if(field==6)bad.sourceToken=0;
            if(field==7){if(action==FusionCommandKind::EquipmentUse)bad.column=32;else bad.column=1;}
            NetBuffer preserved;preserved.WriteU8(0xa5);CHECK(!EncodeFusionCommand(bad,preserved)&&preserved.Size()==1&&preserved.mData[0]==0xa5);
        }
        if(action!=FusionCommandKind::CannonAim){auto bad=command;bad.boardTargetX=1;NetBuffer wire;CHECK(!EncodeFusionCommand(bad,wire));}
    }
    FusionCommand cancel;cancel.epoch=1;cancel.inputEpoch=2;cancel.sequence=3;cancel.action=FusionCommandKind::EquipmentCancel;
    NetBuffer wire;CHECK(EncodeFusionCommand(cancel,wire));cancel.sourceToken=1;NetBuffer bad;CHECK(!EncodeFusionCommand(cancel,bad));
    FusionCommand aim;aim.epoch=1;aim.inputEpoch=2;aim.sequence=3;aim.action=FusionCommandKind::CannonAim;aim.sourceId=1;aim.sourceToken=1;
    aim.boardTargetX=-1;CHECK(!EncodeFusionCommand(aim,bad));aim.boardTargetX=4096;CHECK(!EncodeFusionCommand(aim,bad));
    aim.boardTargetX=4095;aim.boardTargetY=4095;CHECK(EncodeFusionCommand(aim,bad));
    FusionCommand legacy;legacy.epoch=1;legacy.inputEpoch=2;legacy.sequence=3;legacy.action=FusionCommandKind::Ready;legacy.boardTargetY=1;
    NetBuffer legacyWire;CHECK(!EncodeFusionCommand(legacy,legacyWire));
#undef CHECK
    std::printf("FusionTools268CommandTests checks=%d failures=%d\n",checks,failures);return failures;
}
