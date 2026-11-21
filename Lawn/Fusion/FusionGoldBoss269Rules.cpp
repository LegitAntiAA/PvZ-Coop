#include "FusionGoldBoss269Rules.h"
#include "FusionNativeGoldBoss269.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
const FusionGoldBoss269Profile* FindFusionGoldBoss269Profile(uint16_t kind)
{static constexpr FusionGoldBoss269Profile profiles[]={{203,1500,0,1500,400},{223,1500,12000,1500,50}};for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
int FusionGoldBoss269ArmorStage(int value,int maximum,int previous)
{return maximum>0&&value<maximum/3?2:maximum>0&&value>maximum/3&&value<=maximum*int64_t(2)/3?1:previous;}
int FusionGoldPaper269ArmorStage(uint16_t kind,int value,int maximum,int previous)
{
    if(kind!=203)return FusionGoldBoss269ArmorStage(value,maximum,previous);
    if(maximum<=0)return previous;
    int stage=previous;if(value<int(int64_t(maximum)*2/3)&&stage<1)stage=1;if(value<maximum/3&&stage<2)stage=2;return stage;
}
bool TryPlanFusionGoldPaper269Damage(uint16_t kind,const FusionGoldBoss269Armor& a,int resolved,int type,FusionGoldBoss269Damage& output)
{
    if((kind!=203&&kind!=223)||resolved<0||(a.status!=4&&a.status!=5&&a.status!=6)||a.first<0||a.firstMaximum<0||a.first>a.firstMaximum||
       a.second<0||a.secondMaximum<=0||a.second>a.secondMaximum||!std::isfinite(a.body)||a.body<0||(kind==203&&(a.first||a.firstMaximum)))return false;
    FusionGoldBoss269Damage p;p.after=a;p.damage=kind==223?(std::min)(resolved,5000):resolved;p.sourceType=(kind==223?a.status!=6:a.status==4)?0:type;
    int damage=p.damage;
    const auto second=[&](int amount){if(!p.after.second)return amount;const int used=(std::min)(p.after.second,amount);p.after.second-=used;if(!p.after.second){p.secondLost=true;p.after.status=5;}return amount-used;};
    const auto first=[&](int amount){if(!p.after.first)return amount;if(p.after.status!=6)return 0;const int used=(std::min)(p.after.first,amount);p.after.first-=used;if(!p.after.first)p.firstLost=true;return amount-used;};
    const auto body=[&](int amount){if(p.after.status==6&&amount>0){p.bodyDamage=amount;p.after.body=(std::max)(0.f,a.body-float(amount));}};
    const auto firstBody=[&](int amount){if(p.after.first)amount=first(amount);body(amount);};
    switch(p.sourceType)
    {
    case 0:if(a.second&&(damage=second(damage))==0)break;firstBody(damage);break;
    case 1:case 10:if(a.second)second(damage);firstBody(damage);break;
    case 2:if(a.second&&(damage=second(damage))==0)break;p.cold=true;firstBody(damage);break;
    case 3:if(a.second)second(damage);p.cold=true;firstBody(damage);break;
    case 4:firstBody(damage);break;
    case 5:p.cold=true;firstBody(damage);break;
    case 6:if(a.second)second(damage);if(a.first)first(damage);body(damage);break;
    case 11:if(a.second)second(damage);if(float(damage)>=float(p.after.first)+p.after.body)p.directDie=true;else firstBody(damage);break;
    case 12:if(a.second)second(damage);if(a.first&&(damage=first(damage))==0)break;if(float(damage)>p.after.body)p.charred=true;else body(damage);break;
    default:break;
    }
    output=p;return true;
}
bool TryPlanFusionGoldBoss269Damage(const FusionGoldBoss269Armor& a,int resolved,int type,FusionGoldBoss269Damage& out)
{return TryPlanFusionGoldPaper269Damage(223,a,resolved,type,out);}
bool FusionGoldBoss269RejectStatus(int status,bool mc,bool knock,bool freezeButter)
{return mc||knock||(freezeButter&&status==5);}
bool TryStepFusionGoldBoss269Attribute(int status,int count,bool target,FusionGoldBoss269Attribute& output)
{
    if((status!=4&&status!=5&&status!=6)||count<0)return false;FusionGoldBoss269Attribute p;p.count=count;
    if(status==6){if(count==(std::numeric_limits<int>::max)())return false;p.count=count+1;p.shoot=true;if(p.count>3&&target){p.aimedShoot=true;p.count=0;}}
    output=p;return true;
}
bool TryPlanFusionGoldBoss269Shot(bool aimed,bool friendly,bool right,bool dead,bool beforeDying,bool changingRow,FusionGoldBoss269Shot& output)
{
    if(aimed&&friendly)return false;
    if(!aimed&&(dead||beforeDying||changingRow))return false;
    FusionGoldBoss269Shot p;p.aimed=aimed;p.hostile=!friendly;
    if(aimed){p.damage=60;p.way=13;p.sourceYOffset=1;p.velocityMultiplier=1;}
    else p.way=right?0:9;
    output=p;return true;
}
bool TryPlanFusionGoldBoss269Award(int level,FusionGoldBoss269Award& output)
{
    FusionGoldBoss269Award p;
    if(level==1){p.sourceCoinType=0;p.sourceCoinCount=8;p.sourceItemType=35;p.sourceItemCount=20;p.card=256;}
    else if(level==2){p.sourceCoinType=1;p.sourceCoinCount=50;p.sourceItemType=36;p.sourceItemCount=30;p.points=1250;}
    else return false;output=p;return true;
}
bool TryScaleFusionGoldBoss269Health(float body,int first,int second,int round,int difficulty,FusionGoldBoss269Health& output)
{
    if(!std::isfinite(body)||body<0||first<0||second<0||round<0||difficulty<0||difficulty>5)return false;
    const float factor=float(round)*(difficulty==5?.3f:.2f)+1.f;
    const float hp=body*factor,a=float(first)*factor,b=float(second)*factor;
    if(!std::isfinite(hp)||!std::isfinite(a)||!std::isfinite(b)||double(hp)>(std::numeric_limits<int>::max)()||double(a)>(std::numeric_limits<int>::max)()||double(b)>(std::numeric_limits<int>::max)())return false;
    output={hp,int(hp),int(a),int(a),int(b),int(b)};return true;
}
bool TryMakeFusionGoldBoss269AimedShot(float x,float y,float tx,float ty,float py,float& vx,float& vy,float& gravity)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(tx)||!std::isfinite(ty)||!std::isfinite(py)||py<=0)return false;
    // Source gravity is Physics2D.gravity.y=-9.81, multiplied1.5 in the
    // shared solver; birth is Shoot2+Y1 while solver starts from rawShoot2.
    const float g=14.715f*py*.0001f;
    const float a=(tx-x)/150.f,b=(ty-y)/150.f-g*75.f;
    if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(g))return false;
    vx=a;vy=b;gravity=g;return true;
}
bool NativeGoldBoss269StateValid(const NativeGoldBoss269State& s)
{
    const auto& h=s.aimedHead;
    return s.attributeCount>=0&&(s.status==4||s.status==5||s.status==6)&&s.firstStage<=2&&s.secondStage<=2&&s.awardLevel<=2&&s.aimedEvent<=2&&
        std::isfinite(h.mAnimTime)&&h.mAnimTime>=0&&h.mAnimTime<=1&&std::isfinite(h.mAnimRate)&&std::abs(h.mAnimRate)<=10000&&h.mLoopType<=5&&
        LawnNet::NetZombieAnimStateFitsFrames(h,65535)&&
        (h.mPresent||(!h.mDead&&!h.mFrameStart&&!h.mFrameCount&&!h.mAnimTime&&!h.mAnimRate&&!h.mLoopType&&!h.mLoopCount));
}
bool TryMakeFusionGoldPaper269Collider(uint16_t kind,double x,double y,double scale,bool right,double px,double py,FusionGoldPaper269Rect& out)
{
    if((kind!=203&&kind!=223)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(scale)||scale<=0||
        !std::isfinite(px)||px<=0||!std::isfinite(py)||py<=0)return false;
    const double s=scale*(kind==203?.26:.2827066),ox=kind==203?.18603456:-.33830643,
        oy=kind==203?3.8086667:4.4385834,w=kind==203?3.4888055:6.3858414,h=kind==203?7.0271845:8.307423;
    out={x+(ox*(right?-1:1)-w*.5)*s*px,y-(oy+h*.5)*s*py,w*s*px,h*s*py};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.width)&&std::isfinite(out.height);
}
bool NativeGoldPaper269StateValid(uint16_t kind,const NativeGoldBoss269State& s)
{return (kind==203||kind==223)&&NativeGoldBoss269StateValid(s)&&
    (kind==223||(!s.attributeCount&&!s.firstStage&&!s.aimedEvent&&!s.aimedHead.mPresent));}
}
