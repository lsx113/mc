#include "d3dUtil.h"

HRESULT CreateShaderFromFile(
    const WCHAR* csoFileNameInOut,
    const WCHAR* hlslFileName,
    LPCSTR entryPoint,
    LPCSTR shaderModel,
    ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    // 尝试读取已编译的 .cso 文件
    if (csoFileNameInOut && D3DReadFileToBlob(csoFileNameInOut, ppBlobOut) == S_OK)
    {
        return hr;
    }

    // 否则编译 HLSL 文件
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ID3DBlob* errorBlob = nullptr;
    hr = D3DCompileFromFile(hlslFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, shaderModel, dwShaderFlags, 0,
        ppBlobOut, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
        SAFE_RELEASE(errorBlob);
        return hr;
    }

    // 若指定了输出路径，保存 .cso 文件
    if (csoFileNameInOut)
        hr = D3DWriteBlobToFile(*ppBlobOut, csoFileNameInOut, FALSE);

    return hr;
}