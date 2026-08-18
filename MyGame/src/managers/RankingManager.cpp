#include "pch.h"

#include "RankingManager.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <thread>
#include <algorithm>
#include <cstdio>

namespace
{
    // Firebase Realtime Database에 HTTPS 요청 하나를 보내고 응답 본문을 반환한다.
    // 실패하면 빈 문자열. method는 L"GET"(조회) 또는 L"PUT"(등록/덮어쓰기), body는 PUT일 때 보낼 JSON.
    std::string SendJsonRequest(const std::wstring& host, const std::wstring& path, const wchar_t* method, const std::string& body)
    {
        HINTERNET session = WinHttpOpen(L"WobbleBlocksRanking",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
        {
            return "";
        }

        HINTERNET connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection)
        {
            WinHttpCloseHandle(session);
            return "";
        }

        HINTERNET request = WinHttpOpenRequest(connection, method, path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request)
        {
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return "";
        }

        const wchar_t* headers = L"Content-Type: application/json";
        BOOL sendOk = WinHttpSendRequest(request,
            headers, static_cast<DWORD>(-1L),
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);

        std::string responseBody;
        if (sendOk && WinHttpReceiveResponse(request, nullptr))
        {
            DWORD bytesAvailable = 0;
            while (WinHttpQueryDataAvailable(request, &bytesAvailable) && bytesAvailable > 0)
            {
                std::vector<char> buffer(bytesAvailable);
                DWORD bytesRead = 0;
                if (!WinHttpReadData(request, buffer.data(), bytesAvailable, &bytesRead))
                {
                    break;
                }
                responseBody.append(buffer.data(), bytesRead);
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return responseBody;
    }

    // Firebase가 "/rankings.json" 조회에 돌려주는 형태는
    // { "닉네임1": { "name": "닉네임1", "score": 12.3 }, "닉네임2": { ... }, ... } 다.
    // 우리가 SubmitScore로 직접 쓴 형식이라 항목마다 "name"과 "score"가 이 순서로 온다는 걸 알고 있으므로,
    // 범용 JSON 파서 없이 그 두 필드만 순서대로 찾아 읽는다 — 다른 형태의 JSON에는 안 통하는 방식이다.
    std::vector<RankingEntry> ParseRankingsJson(const std::string& json)
    {
        std::vector<RankingEntry> entries;

        const std::string nameKey = "\"name\":\"";
        const std::string scoreKey = "\"score\":";

        size_t searchPos = 0;
        while (true)
        {
            size_t namePos = json.find(nameKey, searchPos);
            if (namePos == std::string::npos)
            {
                break;
            }
            size_t nameStart = namePos + nameKey.length();
            size_t nameEnd = json.find('"', nameStart);
            if (nameEnd == std::string::npos)
            {
                break;
            }

            size_t scorePos = json.find(scoreKey, nameEnd);
            if (scorePos == std::string::npos)
            {
                break;
            }
            size_t scoreStart = scorePos + scoreKey.length();
            size_t scoreEnd = json.find_first_of(",}", scoreStart);
            if (scoreEnd == std::string::npos)
            {
                break;
            }

            RankingEntry entry;
            entry.name = json.substr(nameStart, nameEnd - nameStart);
            entry.score = std::stof(json.substr(scoreStart, scoreEnd - scoreStart));
            entries.push_back(entry);

            searchPos = scoreEnd;
        }

        return entries;
    }

    // "/rankings/닉네임.json" 단건 조회 응답에서 "score" 값만 뽑는다. 그 닉네임이 아직 한 번도
    // 등록된 적 없으면 Firebase가 본문으로 그냥 "null"을 주는데, 그때는 find가 실패하니 0을 반환한다
    // — "최고기록 없음"과 같은 뜻이라 그대로 둬도 된다.
    float ParseScoreField(const std::string& json)
    {
        const std::string scoreKey = "\"score\":";
        size_t scorePos = json.find(scoreKey);
        if (scorePos == std::string::npos)
        {
            return 0.0f;
        }

        size_t scoreStart = scorePos + scoreKey.length();
        size_t scoreEnd = json.find_first_of(",}", scoreStart);
        if (scoreEnd == std::string::npos)
        {
            return 0.0f;
        }

        return std::stof(json.substr(scoreStart, scoreEnd - scoreStart));
    }
}

RankingManager& RankingManager::GetInstance()
{
    static RankingManager instance;
    return instance;
}

void RankingManager::Init(const std::string& databaseUrl)
{
    // WinHttpCrackUrl로 "https://호스트/경로" 형태의 URL에서 호스트 부분만 뽑아둔다.
    // 이후 요청마다 호스트+경로를 새로 조립할 필요 없이 이 m_host만 재사용한다.
    std::wstring wideUrl(databaseUrl.begin(), databaseUrl.end());

    wchar_t hostBuffer[256] = {};
    URL_COMPONENTS urlComponents{};
    urlComponents.dwStructSize = sizeof(urlComponents);
    urlComponents.lpszHostName = hostBuffer;
    urlComponents.dwHostNameLength = sizeof(hostBuffer) / sizeof(hostBuffer[0]);
    urlComponents.dwSchemeLength = static_cast<DWORD>(-1);
    urlComponents.dwUrlPathLength = static_cast<DWORD>(-1);

    if (WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.length()), 0, &urlComponents))
    {
        m_host = hostBuffer;
    }
}

void RankingManager::SubmitScore(const std::string& deviceId, const std::string& name, float heightMeters)
{
    // deviceId를 그대로 Firebase 키로 써서 PUT하면, 같은 컴퓨터에서 다시 등록할 때 자동으로
    // 이전 값을 덮어쓴다(한 컴퓨터당 기록 하나만 남음) — 닉네임을 키로 쓰지 않으므로 서로 다른
    // 컴퓨터가 같은 닉네임을 써도 충돌하지 않는다. 덮어쓰기 전에 먼저 이 컴퓨터의 기존
    // 최고기록을 조회해서, 이번 점수가 그보다 높을 때만 실제로 PUT한다 — "매번 등록"이 아니라
    // "자기 최고기록 갱신했을 때만 등록"이 되게 하려는 것.
    std::wstring host = m_host;

    std::thread([host, deviceId, name, heightMeters]()
    {
        std::wstring wideDeviceId(deviceId.begin(), deviceId.end());
        std::wstring path = L"/rankings/" + wideDeviceId + L".json";

        std::string existingBody = SendJsonRequest(host, path, L"GET", "");
        float bestScore = ParseScoreField(existingBody);

        if (heightMeters <= bestScore)
        {
            return;
        }

        char scoreText[32];
        std::snprintf(scoreText, sizeof(scoreText), "%.2f", heightMeters);

        std::string body = "{\"name\":\"" + name + "\",\"score\":" + scoreText + "}";

        SendJsonRequest(host, path, L"PUT", body);
    }).detach();
}

void RankingManager::RequestRankings()
{
    // RankingManager는 프로그램 전체 수명 동안 살아있는 싱글톤(static 지역 변수)이라,
    // detach된 스레드가 나중에 this로 멤버(m_rankingsMutex/m_cachedRankings)를 건드려도 안전하다.
    std::wstring host = m_host;

    std::thread([host, this]()
    {
        std::string responseBody = SendJsonRequest(host, L"/rankings.json", L"GET", "");
        std::vector<RankingEntry> entries = ParseRankingsJson(responseBody);

        std::sort(entries.begin(), entries.end(), [](const RankingEntry& a, const RankingEntry& b)
        {
            return a.score > b.score;
        });

        std::lock_guard<std::mutex> lock(m_rankingsMutex);
        m_cachedRankings = std::move(entries);
    }).detach();
}

std::vector<RankingEntry> RankingManager::GetCachedRankings() const
{
    std::lock_guard<std::mutex> lock(m_rankingsMutex);
    return m_cachedRankings;
}
