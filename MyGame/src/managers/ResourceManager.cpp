#include "pch.h"

#include "ResourceManager.h"
#include "../core/WindowManager.h"

ResourceManager& ResourceManager::GetInstance()
{
    static ResourceManager instance;
    return instance;
}

void ResourceManager::LoadSprite(const std::string& path)
{
	wstring widePath(path.begin(), path.end());

	// [WIC 디코딩] 파일 -> 디코더 -> 첫 프레임 -> (D2D가 요구하는) 32bpp 미리 곱해진 BGRA로 포맷 변환,
	// 이렇게 변환한 프레임을 그대로 D2D 비트맵으로 만들 수 있다.
	// [실패 처리] 각 단계마다 HRESULT를 확인하고 실패하면 즉시 빠져나온다 — 파일을 못 찾는 등으로
	// 중간 결과가 null인 채로 다음 COM 호출을 이어가면 그 자리에서 바로 크래시가 난다.
	IWICImagingFactory* wicFactory = WindowManager::GetInstance().GetWicFactory();
	if (wicFactory == nullptr)
	{
		return;
	}

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	if (FAILED(wicFactory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())))
	{
		OutputDebugStringA(("ResourceManager::LoadSprite 실패(파일을 못 찾았을 수 있음): " + path + "\n").c_str());
		return;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	if (FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
	{
		return;
	}

	Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
	if (FAILED(wicFactory->CreateFormatConverter(converter.GetAddressOf())))
	{
		return;
	}
	if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		return;
	}

	Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(WindowManager::GetInstance().GetRenderTarget()->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap.GetAddressOf())))
	{
		return;
	}

    SpriteInfo info;
	info.id = path;
	info.bitmap = bitmap;

    m_sprites[path] = info;
}

void ResourceManager::LoadJson(const std::string& path)
{
    // TODO: 실제 JSON 파싱 로직 직접 구현
    (void)path;
}

const SpriteInfo& ResourceManager::GetSpriteInfo(const std::string& id) const
{
    // TODO: m_sprites에서 id로 조회하는 로직 직접 구현
    static const SpriteInfo emptySpriteInfo;
    auto it = m_sprites.find(id);
    return it != m_sprites.end() ? it->second : emptySpriteInfo;
}

void ResourceManager::Shutdown()
{
    // 명시적으로 모든 SpriteInfo의 ComPtr을 해제하여
    // COM이 아직 살아있는(CoUninitialize 전) 상태에서 비트맵이 파괴되도록 보장합니다.
    for (auto& p : m_sprites) {
        p.second.bitmap.Reset();
    }
    m_sprites.clear();
}
