#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace proto {
constexpr std::uint16_t kDefaultPort = 27015;
constexpr int kTickRate = 30;
constexpr int kMaxTeamSize = 5;
constexpr int kWeaponPistol  = 1;
constexpr int kWeaponSMG     = 2;
constexpr int kWeaponShotgun = 3;
constexpr int kWeaponRifle   = 4;
constexpr int kWeaponSniper  = 5;
constexpr int kWeaponCount   = 5;
constexpr int kMaxStatePlayers = 32;
constexpr const char* kT_Register     = "REGISTER";
constexpr const char* kT_Login        = "LOGIN";
constexpr const char* kT_Logout       = "LOGOUT";
constexpr const char* kT_ChatSend     = "CHAT_SEND";
constexpr const char* kT_ChatHistory  = "CHAT_HISTORY";
constexpr const char* kT_QueueJoin    = "QUEUE_JOIN";
constexpr const char* kT_QueueLeave   = "QUEUE_LEAVE";
constexpr const char* kT_Reload       = "RELOAD";
constexpr const char* kT_LeaveMatch   = "LEAVE_MATCH";
constexpr const char* kT_Ping         = "PING";
constexpr const char* kT_LeaderXP     = "LEADER_XP";
constexpr const char* kT_LeaderKills  = "LEADER_KILLS";
constexpr const char* kT_LeaderWin    = "LEADER_WIN";
constexpr const char* kT_StoreList    = "STORE_LIST";
constexpr const char* kT_StoreBuy     = "STORE_BUY";
constexpr const char* kT_WeaponSelect = "WEAPON_SELECT";
constexpr const char* kT_Hello        = "HELLO";
constexpr const char* kT_Err          = "ERR";
constexpr const char* kT_Ok           = "OK";
constexpr const char* kT_RegisterOk   = "REGISTER_OK";
constexpr const char* kT_LoginOk      = "LOGIN_OK";
constexpr const char* kT_ChatMsg      = "CHAT_MSG";
constexpr const char* kT_ChatBatchBeg = "CHAT_BATCH_BEG";
constexpr const char* kT_ChatBatchEnd = "CHAT_BATCH_END";
constexpr const char* kT_QueueStatus  = "QUEUE_STATUS";
constexpr const char* kT_MatchStart   = "MATCH_START";
constexpr const char* kT_MatchPlayer  = "MATCH_PLAYER";
constexpr const char* kT_MatchEnd     = "MATCH_END";
constexpr const char* kT_StoreItem    = "STORE_ITEM";
constexpr const char* kT_StoreEnd     = "STORE_END";
constexpr const char* kT_StoreBought  = "STORE_BOUGHT";
constexpr const char* kT_WeaponOk     = "WEAPON_OK";
constexpr const char* kT_Pong         = "PONG";
constexpr const char* kT_LeaderRow    = "LEADER_ROW";
constexpr const char* kT_LeaderEnd    = "LEADER_END";
constexpr const char* kT_Reconnected  = "RECONNECTED";
constexpr const char* kU_Input        = "INPUT";
constexpr const char* kU_State        = "STATE";
constexpr const char* kU_Event        = "EVENT";
std::string urlEncode(const std::string& s);
std::string urlDecode(const std::string& s);
std::vector<std::string> splitDecode(const std::string& line);
std::string encodeLine(const std::vector<std::string>& fields);
}
