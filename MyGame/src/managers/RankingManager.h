#pragma once

#include <string>
#include <vector>
#include <mutex>

#include "../util/Types.h"

// Firebase Realtime Database와 REST(WinHTTP)로 통신해 온라인 랭킹을 올리고/받아오는 싱글톤.
// 네트워크 요청은 응답이 올 때까지 몇백ms씩 걸릴 수 있어, 게임 루프(WindowManager::MessageLoop)를
// 멈추지 않도록 항상 별도 스레드(std::thread)에서 수행한다. 그 스레드가 결과를 뮤텍스로 보호된
// 캐시(m_cachedRankings)에 써두면, 게임 루프 쪽은 그 캐시를 읽기만 한다 — 두 스레드가 동시에
// 같은 데이터를 건드리는 걸 뮤텍스 하나로 막는 구조.
class RankingManager
{
public:
    static RankingManager& GetInstance();

    // Firebase Realtime Database REST 주소(예: "https://xxx-default-rtdb.firebaseio.com/")를 등록한다.
    // 게임 시작 시(MyGame.cpp) 한 번만 호출하면 된다.
    void Init(const std::string& databaseUrl);

    // 별도 스레드에서 점수를 등록한다. deviceId(컴퓨터별 고유 ID, PlayerManager::GetDeviceId())를
    // 그대로 DB의 키로 쓰므로, 같은 deviceId로 다시 호출하면 이전 값을 덮어쓴다. name(닉네임)은
    // 화면 표시용으로 같이 저장될 뿐 키로는 쓰이지 않는다 — 그래야 서로 다른 컴퓨터가 같은
    // 닉네임을 쓰더라도 랭킹 기록이 서로 덮어써지지 않는다. 호출 즉시 리턴하고, 실제 전송은
    // 백그라운드에서 진행된다.
    void SubmitScore(const std::string& deviceId, const std::string& name, float heightMeters);

    // 별도 스레드에서 전체 랭킹을 새로 받아온다. 완료되면 GetCachedRankings()가 갱신된 값을 반환한다.
    // 호출 즉시 리턴한다 — 랭킹 패널을 열 때마다 불러서 "다음에 열 때는 최신 값"이 보이게 하는 식으로 쓴다.
    void RequestRankings();

    // 마지막으로 성공적으로 받아온 랭킹 목록(점수 내림차순)의 복사본을 반환한다.
    // 아직 한 번도 못 받아왔으면 빈 벡터.
    std::vector<RankingEntry> GetCachedRankings() const;

private:
    RankingManager() = default;
    ~RankingManager() = default;
    RankingManager(const RankingManager&) = delete;
    RankingManager& operator=(const RankingManager&) = delete;

    std::wstring m_host;

    mutable std::mutex m_rankingsMutex;
    std::vector<RankingEntry> m_cachedRankings;
};
