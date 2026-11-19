#include <unity.h>
#include "missionlink_packet.h"
#include "missionlink_config.h"
#include "mesh_router.h"
#include <string.h>
static const uint8_t A[8]={1,0,0,0,0,0,0,1}, B[8]={2,0,0,0,0,0,0,2}, C[8]={3,0,0,0,0,0,0,3};
static ml::Packet makePacket(ml::AddressMode mode=ml::AddressMode::Direct){
 ml::Packet p; p.mode=mode; memcpy(p.source,A,8); memcpy(p.destination,C,8); p.messageId[0]=0x42;
 p.hopLimit=4; p.payload={'t','e','s','t'}; return p;
}
void test_preset_names_and_parser(){ ml::NodePreset p; TEST_ASSERT_TRUE(ml::parsePreset("client",p)); TEST_ASSERT_EQUAL((int)ml::NodePreset::Client,(int)p); TEST_ASSERT_TRUE(ml::parsePreset("repeater",p)); TEST_ASSERT_EQUAL((int)ml::NodePreset::Repeater,(int)p); TEST_ASSERT_TRUE(ml::parsePreset("gateway",p)); TEST_ASSERT_EQUAL((int)ml::NodePreset::Gateway,(int)p); TEST_ASSERT_FALSE(ml::parsePreset("other",p)); }
void test_client_does_not_relay_unicast(){ ml::MeshRouter r(ml::NodePreset::Client); auto p=makePacket(); ml::Packet f; TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,10,f)); }
void test_mesh_direct_addressed_to_local_delivers(){
 ml::MeshRouter r; auto p=makePacket(); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal,(int)r.process(p,C,10,f));
}
void test_mesh_per_source_rate_limit(){
 ml::MeshRouter r(ml::NodePreset::Repeater); ml::Packet f;
 for(uint8_t i=1;i<=ml::MeshRouter::MAX_FORWARDS_PER_SOURCE_WINDOW;++i){
  auto p=makePacket(); p.messageId[7]=i;
  TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,100,f));
 }
 auto p=makePacket(); p.messageId[7]=99;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,100,f));
 p.messageId[7]=100;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,1100,f));
}
void test_mesh_rejects_packet_from_local_source(){
 ml::MeshRouter r; auto p=makePacket(); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,A,10,f));
}
void test_repeater_forwards_group_without_local_delivery(){ ml::MeshRouter r(ml::NodePreset::Repeater); auto p=makePacket(ml::AddressMode::Broadcast); ml::Packet f; TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,10,f)); TEST_ASSERT_EQUAL_UINT8(1,f.hopCount); }
void test_gateway_delivers_and_forwards_group(){ ml::MeshRouter r(ml::NodePreset::Gateway); auto p=makePacket(ml::AddressMode::Broadcast); ml::Packet f; TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverAndForward,(int)r.process(p,B,10,f)); }
void test_mesh_three_node_forwarding_increments_once(){ ml::MeshRouter r(ml::NodePreset::Repeater); auto p=makePacket(); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,10,f)); TEST_ASSERT_EQUAL_UINT8(1,f.hopCount);
 ml::MeshRouter atC;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal,(int)atC.process(f,C,11,p)); }
void test_mesh_duplicate_suppressed(){ ml::MeshRouter r(ml::NodePreset::Gateway); auto p=makePacket(ml::AddressMode::Broadcast); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverAndForward,(int)r.process(p,B,10,f));
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropDuplicate,(int)r.process(p,B,11,f)); }
void test_mesh_hop_budget_stops_forwarding(){ ml::MeshRouter r; auto p=makePacket(); p.hopCount=p.hopLimit; ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,10,f)); }
void test_mesh_broadcast_delivers_at_hop_limit(){ ml::MeshRouter r; auto p=makePacket(ml::AddressMode::Broadcast); p.hopCount=p.hopLimit; ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal,(int)r.process(p,B,10,f)); }
void test_mesh_rejects_zero_source_and_message_id(){ ml::MeshRouter r; auto p=makePacket(); memset(p.source,0,8); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,10,f)); p=makePacket(); memset(p.messageId,0,8);
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,10,f)); }
void test_mesh_rejects_group_without_group_id(){ ml::MeshRouter r; auto p=makePacket(ml::AddressMode::Group); memset(p.destination,0,8); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,10,f)); }
void test_mesh_allows_broadcast_without_destination_id(){ ml::MeshRouter r; auto p=makePacket(ml::AddressMode::Broadcast); memset(p.destination,0,8); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverLocal,(int)r.process(p,B,10,f)); }
void test_mesh_global_rate_limit(){ ml::MeshRouter r(ml::NodePreset::Repeater); ml::Packet f;
 for(uint8_t i=1;i<=ml::MeshRouter::MAX_FORWARDS_PER_WINDOW;++i){auto p=makePacket();p.messageId[7]=i;p.source[7]=i;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,100,f));}
 auto p=makePacket();p.messageId[7]=99; TEST_ASSERT_EQUAL((int)ml::MeshAction::DropInvalid,(int)r.process(p,B,100,f));
 p.messageId[7]=100;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::ForwardOnly,(int)r.process(p,B,1100,f)); }
void test_mesh_duplicate_cache_expires(){ ml::MeshRouter r(ml::NodePreset::Gateway); auto p=makePacket(ml::AddressMode::Broadcast); ml::Packet f;
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverAndForward,(int)r.process(p,B,10,f));
 TEST_ASSERT_EQUAL((int)ml::MeshAction::DeliverAndForward,(int)r.process(p,B,10+ml::MeshRouter::DUPLICATE_TTL_MS,f)); }
void test_round_trip(){ auto p=makePacket(ml::AddressMode::Group);std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));ml::Packet q;
 TEST_ASSERT_TRUE(ml::decode(b.data(),b.size(),q));TEST_ASSERT_EQUAL_UINT8((uint8_t)p.type,(uint8_t)q.type);TEST_ASSERT_EQUAL_UINT8((uint8_t)p.mode,(uint8_t)q.mode); }
void test_reject_truncated(){uint8_t b[]={1,2,0};ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b,sizeof(b),q));}
void test_reject_null_input(){ml::Packet q;TEST_ASSERT_FALSE(ml::decode(nullptr,32,q));}
void test_reject_invalid_hops(){ml::Packet p;p.hopLimit=1;p.hopCount=2;std::vector<uint8_t>b;TEST_ASSERT_FALSE(ml::encode(p,b));}
void test_reject_invalid_type_on_encode(){ml::Packet p;p.type=(ml::PacketType)99;std::vector<uint8_t>b;TEST_ASSERT_FALSE(ml::encode(p,b));}
void test_reject_invalid_mode_on_encode(){ml::Packet p;p.mode=(ml::AddressMode)99;std::vector<uint8_t>b;TEST_ASSERT_FALSE(ml::encode(p,b));}
void test_reject_oversized_payload(){ml::Packet p;p.payload.resize(ML_MAX_PAYLOAD+1,0);std::vector<uint8_t>b;TEST_ASSERT_FALSE(ml::encode(p,b));}
void test_encode_failure_clears_stale_output(){ml::Packet p;std::vector<uint8_t>b={0xde,0xad};p.version=99;TEST_ASSERT_FALSE(ml::encode(p,b));TEST_ASSERT_TRUE(b.empty());}
void test_max_payload_round_trip(){ml::Packet p;p.payload.resize(ML_MAX_PAYLOAD);for(size_t i=0;i<p.payload.size();++i)p.payload[i]=(uint8_t)i;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));TEST_ASSERT_EQUAL_UINT32(32+ML_MAX_PAYLOAD,b.size());ml::Packet q;TEST_ASSERT_TRUE(ml::decode(b.data(),b.size(),q));TEST_ASSERT_EQUAL_UINT32(ML_MAX_PAYLOAD,q.payload.size());}
void test_decode_rejects_unknown_version(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[0]=2;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_invalid_type(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[1]=99;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_invalid_mode(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[2]=99;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_hop_limit_over_max(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[28]=ML_MAX_HOPS+1;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_hop_count_over_limit(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[29]=5;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_payload_length_mismatch(){ml::Packet p;p.payload={'o','k'};std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b[31]=3;ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_decode_rejects_trailing_bytes(){ml::Packet p;std::vector<uint8_t>b;TEST_ASSERT_TRUE(ml::encode(p,b));b.push_back(0);ml::Packet q;TEST_ASSERT_FALSE(ml::decode(b.data(),b.size(),q));}
void test_hex_id_null_is_safe(){TEST_ASSERT_EQUAL_STRING("",ml::hexId(nullptr,8).c_str());}
int main(int argc,char**argv){UNITY_BEGIN();
 RUN_TEST(test_preset_names_and_parser); RUN_TEST(test_client_does_not_relay_unicast); RUN_TEST(test_mesh_direct_addressed_to_local_delivers); RUN_TEST(test_mesh_per_source_rate_limit); RUN_TEST(test_mesh_rejects_packet_from_local_source); RUN_TEST(test_repeater_forwards_group_without_local_delivery); RUN_TEST(test_gateway_delivers_and_forwards_group); RUN_TEST(test_mesh_three_node_forwarding_increments_once);RUN_TEST(test_mesh_duplicate_suppressed);RUN_TEST(test_mesh_hop_budget_stops_forwarding);
 RUN_TEST(test_mesh_broadcast_delivers_at_hop_limit);RUN_TEST(test_mesh_rejects_zero_source_and_message_id);RUN_TEST(test_mesh_rejects_group_without_group_id);RUN_TEST(test_mesh_allows_broadcast_without_destination_id);RUN_TEST(test_mesh_global_rate_limit);RUN_TEST(test_mesh_duplicate_cache_expires);
 RUN_TEST(test_round_trip);RUN_TEST(test_reject_truncated);RUN_TEST(test_reject_null_input);RUN_TEST(test_reject_invalid_hops);RUN_TEST(test_reject_invalid_type_on_encode);
 RUN_TEST(test_reject_invalid_mode_on_encode);RUN_TEST(test_reject_oversized_payload);RUN_TEST(test_encode_failure_clears_stale_output);RUN_TEST(test_max_payload_round_trip);
 RUN_TEST(test_decode_rejects_unknown_version);RUN_TEST(test_decode_rejects_invalid_type);RUN_TEST(test_decode_rejects_invalid_mode);RUN_TEST(test_decode_rejects_hop_limit_over_max);
 RUN_TEST(test_decode_rejects_hop_count_over_limit);RUN_TEST(test_decode_rejects_payload_length_mismatch);RUN_TEST(test_decode_rejects_trailing_bytes);RUN_TEST(test_hex_id_null_is_safe);
 return UNITY_END();}
