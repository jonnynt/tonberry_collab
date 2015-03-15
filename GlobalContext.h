//
// GlobalContext.h
//
// Definition of global context.  The AI must use a global context because it is embeeded inside a DLL
// Written by Matthew Fisher
//

#ifdef ULTRA_FAST
#define g_ReportingEvents 0
#else
extern bool g_ReportingEvents;
#endif

class GraphicsInfo
{
public:
    void Init();
    void SetDevice(LPDIRECT3DDEVICE9 Device);
    bool OverlayValid()
    {
        return (_Overlay != NULL);
    }
    __forceinline ID3D9DeviceOverlay& Overlay()
    {
        Assert(_Overlay != NULL, "Overlay NULL");
        return *_Overlay;
    }

    //
    // Accessors
    //
    __forceinline LPDIRECT3DDEVICE9 Device()
    {
        Assert(_Device != NULL, "Device == NULL");
        return _Device;
    }
    __forceinline const D3DDEVICE_CREATION_PARAMETERS& CreationParameters()
    {
        return _CreationParameters;
    }
    __forceinline const D3DPRESENT_PARAMETERS& PresentParameters()
    {
        return _PresentParameters;
    }
    __forceinline const Vec2i& WindowDimensions()
    {
        return _WindowDimensions;
    }
    __forceinline const RECT& WindowRect()
    {
        return _WindowRect;
    }

private:
    ID3D9DeviceOverlay              *_Overlay;
    LPDIRECT3DDEVICE9               _Device;
    D3DPRESENT_PARAMETERS           _PresentParameters;
    D3DDEVICE_CREATION_PARAMETERS   _CreationParameters;
    RECT                            _ClientRect, _WindowRect;
    Vec2i                           _WindowDimensions;
};

struct GlobalContext
{
    GlobalContext(){}
    void Init();
	void UnlockTexture(D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle);
	void CreateTexture(D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle, IDirect3DTexture9** ppTexture);
	void UnlockRect(D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle);
	void UpdateSurface(D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle);
	void Destroy(HANDLE Handle);
	bool SetTexture(DWORD Stage, HANDLE *SurfaceHandles, UINT SurfaceHandleCount);
	void BeginScene();

    GraphicsInfo Graphics;
};

extern GlobalContext *g_Context;
