#include "mesh_router.h"
#include "missionlink_config.h"
#include <string.h>
namespace ml {
namespace {
bool same8(const uint8_t a[8], const uint8_t b[8]) { return memcmp(a,b,8)==0; }
bool allZero(const uint8_t a[8]) { uint8_t v=0; for(size_t i=0;i<8;++i) v|=a[i]; return v==0; }
bool validType(uint8_t t) { return t>=static_cast<uint8_t>(PacketType::Hello)&&t<=static_cast<uint8_t>(PacketType::Leave); }
bool validMode(uint8_t m) { return m<=static_cast<uint8_t>(AddressMode::Broadcast); }
}
const char* presetName(NodePreset p) { switch(p) { case NodePreset::Client:return "client"; case NodePreset::Repeater:return "repeater"; case NodePreset::Gateway:return "gateway"; } return "client"; }
bool parsePreset(const char* n, NodePreset& out) { if(!n)return false; if(!strcmp(n,"client")){out=NodePreset::Client;return true;} if(!strcmp(n,"repeater")){out=NodePreset::Repeater;return true;} if(!strcmp(n,"gateway")){out=NodePreset::Gateway;return true;} return false; }
MeshRouter::MeshRouter(NodePreset p):preset_(p){ reset(); }
void MeshRouter::setPreset(NodePreset p){ preset_=p; reset(); }
void MeshRouter::reset(){
 memset(seen_,0,sizeof(seen_)); memset(sources_,0,sizeof(sources_));
 nextSeen_=nextSource_=0; globalWindowStart_=0; globalForwards_=0; globalWindowValid_=false;
}
bool MeshRouter::isDuplicate(const Packet& p,uint32_t now){
 for(size_t i=0;i<DUPLICATE_CACHE_SIZE;++i){
  SeenEntry& e=seen_[i];
  if(e.valid && static_cast<uint32_t>(now-e.seenAt)>=DUPLICATE_TTL_MS) e.valid=false;
  if(e.valid && same8(e.source,p.source) && same8(e.messageId,p.messageId)) return true;
 }
 SeenEntry& e=seen_[nextSeen_]; memcpy(e.source,p.source,8); memcpy(e.messageId,p.messageId,8);
 e.seenAt=now; e.valid=true; nextSeen_=(nextSeen_+1)%DUPLICATE_CACHE_SIZE; return false;
}
bool MeshRouter::allowForward(const Packet& p,uint32_t now){
 if(!globalWindowValid_ || static_cast<uint32_t>(now-globalWindowStart_)>=RATE_WINDOW_MS){
  globalWindowStart_=now; globalForwards_=0; globalWindowValid_=true;
 }
 if(globalForwards_>=MAX_FORWARDS_PER_WINDOW) return false;
 SourceBucket* b=nullptr;
 for(size_t i=0;i<SOURCE_BUCKETS;++i) if(sources_[i].valid&&same8(sources_[i].source,p.source)){b=&sources_[i];break;}
 if(!b){ b=&sources_[nextSource_]; nextSource_=(nextSource_+1)%SOURCE_BUCKETS; memcpy(b->source,p.source,8); b->windowStart=now; b->forwards=0; b->valid=true; }
 else if(static_cast<uint32_t>(now-b->windowStart)>=RATE_WINDOW_MS){b->windowStart=now;b->forwards=0;}
 if(b->forwards>=MAX_FORWARDS_PER_SOURCE_WINDOW) return false;
 ++b->forwards; ++globalForwards_; return true;
}
MeshAction MeshRouter::process(const Packet& p,const uint8_t localId[8],uint32_t now,Packet& forwarded){
 if(!localId || p.version!=ML_PROTOCOL_VERSION || !validType(static_cast<uint8_t>(p.type)) ||
    !validMode(static_cast<uint8_t>(p.mode)) || p.payload.size()>ML_MAX_PAYLOAD ||
    p.hopLimit>ML_MAX_HOPS || p.hopCount>p.hopLimit || allZero(p.source) ||
    allZero(p.messageId) || same8(p.source,localId)) return MeshAction::DropInvalid;
 if((p.mode==AddressMode::Direct || p.mode==AddressMode::Group) && allZero(p.destination)) return MeshAction::DropInvalid;
 if(isDuplicate(p,now)) return MeshAction::DropDuplicate;
 const bool directHere=p.mode==AddressMode::Direct && same8(p.destination,localId);
 if(directHere) return MeshAction::DeliverLocal;
 const bool localBroadcast=p.mode!=AddressMode::Direct;
 if(preset_==NodePreset::Client) return localBroadcast?MeshAction::DeliverLocal:MeshAction::DropInvalid;
 if(p.hopCount>=p.hopLimit) return localBroadcast&&preset_==NodePreset::Gateway?MeshAction::DeliverLocal:MeshAction::DropInvalid;
 if(!allowForward(p,now)) return localBroadcast&&preset_==NodePreset::Gateway?MeshAction::DeliverLocal:MeshAction::DropInvalid;
 forwarded=p; ++forwarded.hopCount;
 if(localBroadcast && preset_==NodePreset::Gateway) return MeshAction::DeliverAndForward;
 return MeshAction::ForwardOnly;
}
}
