#include "FusionBoss361Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
bool FusionBoss361Kind(uint16_t k){return k==44||k==46;}
bool FusionBoss361ViewValid(const FusionBoss361View& s)
{
 return FusionBoss361Kind(s.kind)&&uint8_t(s.phase)<=8&&s.targetRow<5&&s.targetColumn<3&&s.ballType<2&&s.bodyStage<3&&s.eyeMask<4&&s.bodyHistoryMask<4&&
  std::isfinite(s.rvOffsetX)&&std::isfinite(s.rvOffsetY)&&std::abs(s.rvOffsetX)<=100&&std::abs(s.rvOffsetY)<=100&&
  std::isfinite(s.eyeBlackSeconds)&&s.eyeBlackSeconds>=0&&s.eyeBlackSeconds<=.75f&&(!s.eyeBlackSeconds||s.dying)&&
  s.summonCount>=0&&s.summonCount<=1000000&&s.spawnCount>=0&&s.spawnCount<=8&&s.summonBallCount>=0&&s.summonBallCount<=1000000&&
  std::isfinite(s.summonTime)&&s.summonTime>=0&&s.summonTime<=5&&std::isfinite(s.ballCountdown)&&s.ballCountdown>=0&&s.ballCountdown<=10&&
  (s.dying?(s.sourceStatus==0||s.sourceStatus==1||s.sourceStatus==21):((s.sourceStatus==0)==(s.phase==FusionBoss361Phase::HeadIdle))&&
   (s.sourceStatus==0||s.sourceStatus==21))&&(!s.colliderEnabled||s.sourceStatus==0);
}
bool FusionBoss361AllowChild(uint16_t k,bool boss2)
{
 // These source type tables are intentionally exact, not a native class guess.
 switch(k){case 25:case 26:case 29:case 119:case 205:case 206:
 case 11:case 12:case 13:case 14:case 17:case 19:case 27:case 113:case 200:case 214:return false;}
 if(!boss2)
 {
  if(k<29)return ((0x10000128u>>k)&1u)==0;
  if(k<49)return k!=37&&(k<41||k>48);
  return k!=212&&(k<218||k>224);
 }
 if(k<19)return ((0x50400u>>k)&1u)==0;
 switch(int(k)-28){case 0:case 6:case 8:case 9:case 11:case 12:case 13:case 14:case 15:case 16:case 17:case 18:case 19:case 20:return false;}
 return k!=212&&(k<218||k>224);
}
bool FusionBoss361BuildChildPool(bool boss2,std::span<const uint16_t> prefabs,std::span<uint16_t> output,uint16_t& count)
{
 uint16_t n=0;for(uint16_t k=0;k<99;++k)if(std::find(prefabs.begin(),prefabs.end(),k)!=prefabs.end()&&FusionBoss361AllowChild(k,boss2))++n;
 if(n>output.size())return false;
 uint16_t at=0;for(uint16_t k=0;k<99;++k)if(std::find(prefabs.begin(),prefabs.end(),k)!=prefabs.end()&&FusionBoss361AllowChild(k,boss2))output[at++]=k;
 count=n;return true;
}
bool PlanFusionBoss361Tick(const FusionBoss361View& s,const FusionBoss361Input& in,float dt,FusionBoss361Plan& out)
{
 if(!FusionBoss361ViewValid(s)||!std::isfinite(dt)||dt<0||dt>1||!std::isfinite(in.freezeSpeed)||in.freezeSpeed<0||in.firstCrushPlantRow< -1||in.firstCrushPlantRow>4)return false;
 FusionBoss361Plan p;p.next=s;
 if(!in.gameActive){out=p;return true;}
 if(s.phase==FusionBoss361Phase::Idle)
 {
  if(s.spawnCount<1)p.decision=FusionBoss361Decision::HeadEnter;
  else
  {
   p.next.summonTime-=dt;
   if(p.next.summonTime<0)
   {p.next.summonTime=4;if(in.firstCrushPlantRow>=0){p.decision=FusionBoss361Decision::Stomp;p.next.targetRow=uint8_t(in.firstCrushPlantRow);}else{p.decision=FusionBoss361Decision::Spawn;p.randomRowUpper=5;}}
  }
 }
 if(s.phase==FusionBoss361Phase::HeadIdle&&in.freezeSpeed>0&&!s.ballBusy)
 {
  p.next.ballCountdown-=dt;
  if(p.next.ballCountdown<0)
  {p.next.ballCountdown=10;p.decision=FusionBoss361Decision::Attack;p.randomRowUpper=5;p.randomBallType=!in.droppedCard16||in.droppedCard10;if(!p.randomBallType)p.next.ballType=1;}
 }
 out=p;return true;
}
bool PlanFusionBoss361Skill(const FusionBoss361View& s,float hp,int maximum,FusionBoss361Plan& out)
{
 if(!FusionBoss361ViewValid(s)||maximum<=0||!std::isfinite(hp))return false;
 FusionBoss361Plan p;p.next=s;
 if(hp>float(maximum/2))p.decision=FusionBoss361Decision::Bungi;
 else{p.decision=FusionBoss361Decision::Rv;p.randomRowUpper=4;p.randomColumnUpper=3;}
 out=p;return true;
}
bool ResolveFusionBoss361Plan(const FusionBoss361Plan& p,int row,int col,int ball,FusionBoss361View& out,uint64_t& main,uint64_t& driver)
{
 if((p.randomRowUpper&&(row<0||row>=p.randomRowUpper))||(p.randomColumnUpper&&(col<0||col>=p.randomColumnUpper))||(p.randomBallType&&(ball<0||ball>1)))return false;
 auto s=p.next;uint64_t m=0,d=0;using P=FusionSourceGraph313Param;
 if(p.randomRowUpper)s.targetRow=uint8_t(row);if(p.randomColumnUpper)s.targetColumn=uint8_t(col);if(p.randomBallType)s.ballType=uint8_t(ball);
 const auto trigger=[&](P parameter){m|=FusionSourceGraph313Trigger(parameter);};
 switch(p.decision)
 {
 case FusionBoss361Decision::None:break;
 case FusionBoss361Decision::HeadEnter:s.phase=FusionBoss361Phase::HeadEnter;s.ballBusy=false;trigger(P::BossHeadEnter);break;
 case FusionBoss361Decision::Spawn:s.phase=FusionBoss361Phase::Spawn;trigger(P(uint8_t(P::BossSpawn1)+s.targetRow));break;
 case FusionBoss361Decision::Stomp:s.phase=FusionBoss361Phase::Foot;trigger(P(uint8_t(P::BossStomp1)+(std::min)(int(s.targetRow),3)));break;
 case FusionBoss361Decision::Attack:s.ballBusy=true;s.eyeMask|=uint8_t(1u<<s.ballType);trigger(P(uint8_t(P::BossAttack1)+s.targetRow));d=FusionSourceGraph313Trigger(P::BossDrive);break;
 case FusionBoss361Decision::Bungi:s.phase=FusionBoss361Phase::Bungi;trigger(P::BossBungi);break;
 case FusionBoss361Decision::Rv:s.phase=FusionBoss361Phase::Rv;trigger(P::BossRv);break;
 default:return false;
 }
 if(!FusionBoss361ViewValid(s))return false;out=s;main=m;driver=d;return true;
}
bool PlanFusionBoss361BodyStage(uint16_t kind,int max,float hp,uint8_t old,uint8_t& stage)
{
 if(!FusionBoss361Kind(kind)||max<=0||!std::isfinite(hp)||old>2)return false;
 const int lo=max/3,hi=int((int64_t(max)*2)/3);uint8_t next=old;
 if(float(lo)<hp&&hp<=float(hi))next=1;else if(hp<float(lo))next=2;
 stage=next;return true;
}
bool FusionBoss361StompCell(int target,int row,int col,bool tag0)
{return target>=0&&target<5&&row>=0&&row<5&&std::abs(row-target)<2&&col>5&&tag0;}
bool FusionBoss361RvCell(int tr,int tc,int row,int col)
{return tr>=0&&tr<4&&tc>=0&&tc<3&&row>=tr&&row<=tr+1&&col>=tc&&col<=tc+2;}
bool ApplyFusionBoss361Behaviour(FusionBoss361View& s,FusionSourceGraph313Event event,int count)
{
 if(!FusionBoss361ViewValid(s))return false;auto next=s;using E=FusionSourceGraph313Event;
 switch(event)
 {
 case E::BossIdleEnter:next.phase=FusionBoss361Phase::Idle;next.summonTime=4;break;
 case E::BossHeadEnterExit:next.phase=FusionBoss361Phase::HeadIdle;next.sourceStatus=0;next.ballCountdown=3;next.colliderEnabled=true;break;
 case E::BossHeadAttackExit:if(count<4||count>=8)return false;next.spawnCount=count;next.eyeMask=0;break;
 case E::BossHeadLeaveEnter:next.phase=FusionBoss361Phase::HeadLeave;next.sourceStatus=21;next.colliderEnabled=false;break;
 case E::BossHeadLeaveExit:next.phase=FusionBoss361Phase::Idle;break;
 case E::BossRvEnter:next.rvVisible=true;break;
 case E::BossRvExit:next.rvVisible=false;break;
 default:return false;
 }
 if(!FusionBoss361ViewValid(next))return false;s=next;return true;
}
}
