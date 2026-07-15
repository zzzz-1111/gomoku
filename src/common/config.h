#ifndef CONFIG_H
#define CONFIG_H

namespace gomoku_config {

constexpr int kMainWindowWidth = 1200;
constexpr int kMainWindowHeight = 824;
constexpr int kMainWindowMinWidth = 900;
constexpr int kMainWindowMinHeight = 620;
constexpr int kHomePageIndex = 0;
constexpr int kGamePageIndex = 1;
constexpr int kBoardSize = 15;
constexpr int kDefaultCellSize = 40;
constexpr int kDefaultMargin = 24;
constexpr int kDefaultPort = 5000;
constexpr int kDefaultDiscoveryPort = 45454;
constexpr int kRoomBroadcastIntervalMs = 1000;
constexpr int kDefaultReplayDelayMs = 300;

}

#endif
