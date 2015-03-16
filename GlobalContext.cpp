#include "Main.h"
#include <stdint.h>
#include <sstream>
#include <boost\filesystem.hpp>
#include "cachemap.h"
#include <Windows.h>

#define DEBUG 1

#if DEBUG
double PCFreq = 0.0;
__int64 CounterStart = 0;
string debug_file = "tonberry\\tests\\timing.csv";
ofstream debug(debug_file, ofstream::out | ofstream::app);

void StartCounter()
{
    LARGE_INTEGER li;
    if(!QueryPerformanceFrequency(&li))
	cout << "QueryPerformanceFrequency failed!\n";

    PCFreq = double(li.QuadPart)/1000000.0;

    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
}

double GetCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return double(li.QuadPart-CounterStart)/PCFreq;
}
#endif
/*********************************
*								 *
*$(SolutionDir)$(Configuration)\ *
*								 *
*********************************/
//-------------
/*#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>*/
//-------------

#ifndef ULTRA_FAST
bool g_ReportingEvents = false;
#endif

namespace fs = boost::filesystem;

GlobalContext *g_Context;

void GraphicsInfo::Init()
{
    _Device = NULL;
    _Overlay = NULL;
}

void GraphicsInfo::SetDevice(LPDIRECT3DDEVICE9 Device)
{
    Assert(Device != NULL, "Device == NULL");
    D3D9Base::IDirect3DSwapChain9* pSwapChain;    
    HRESULT hr = Device->GetSwapChain(0, &pSwapChain);
    Assert(SUCCEEDED(hr), "GetSwapChain failed");    
    hr = pSwapChain->GetPresentParameters(&_PresentParameters);
    Assert(SUCCEEDED(hr), "GetPresentParameters failed");    
    pSwapChain->Release();
    hr = Device->GetCreationParameters(&_CreationParameters);
    Assert(SUCCEEDED(hr), "GetCreationParameters failed");
    _Device = Device;
}

template <typename T>
T ToNumber(const std::string& Str)	//convert string to unsigned long long -> uint64_t
{
    T Number;
    std::stringstream S(Str);
	S >> Number;
    return Number;
}
//Debug control variable
bool debugmode = false;

//Global Variables
//vector<int> pixval;
int pixval[64];
//vector<int> pixval2;
int pixval2[98];
//vector<int> objval;
int objval[64];
unordered_map<uint64_t, string> hashmap;	//Hashmap of unique hashvals 
unordered_map<uint64_t, string> collmap;	//Hashmap of collision duplicates
unordered_map<string, string> coll2map;	//Hashmap of collision hashvals after algorithm 2
unordered_map<uint64_t, string> objmap;
unordered_map<uint64_t, string>::iterator it;
unordered_map<string, string>::iterator it2;
uint64_t hashval; //current hashval of left half of memory
uint64_t objtop; //object in top left corner of memory
uint64_t objbot; //object in bottom left corner of memory
BigInteger hashval2; //hashval for algo 2

float resize_factor;
string texdir("");

//TextureCache
unsigned cache_size = 1000;
TextureCache * texcache;

void initCache(){
	texcache = new TextureCache(cache_size);
}

void loadprefs ()
{
	resize_factor = 4.0;
	ifstream prefsfile;
	prefsfile.open ("tonberry\\prefs.txt", ifstream::in);
	if (prefsfile.is_open()){
		string line;
		while ( getline(prefsfile, line) ){
			if (line.substr(0, line.find("=")) == "resize_factor")
				resize_factor = (float)atoi(line.substr(line.find("=") + 1, line.length()).c_str()); //make sure no spaces in prefs file
			if (line.substr(0, line.find("=")) == "debug_mode")
				debugmode = (line.substr(line.find("=")+1, line.length()) == string("yes"));
			if (line.substr(0, line.find("=")) == "cache_size")
				cache_size = (unsigned)atoi(line.substr(line.find("=") + 1, line.length()).c_str());
			if (line.substr(0, line.find("=")) == "texture_dir"){
				texdir = line.substr(line.find("=")+1, line.length());
				if(texdir.back() != '/') texdir += "/";
			}
			//if (line.substr(0) == string("#"));
		}
		prefsfile.close();
	}
	ofstream check;
	check.open("tonberry\\tests\\testprefs.txt", ofstream::out);
	check << "Texdir is: " << texdir;
	check.close();
}

//Mod Jay
void loadhashfile ()	//Expects hash1map folder to be in ff8/tonberry directory
{
	fs::path hashpath("tonberry/hashmap");
	if(!fs::exists(hashpath)){
		ofstream err;								//Error reporting file
		err.open("tonberry/error.txt", ofstream::out | ofstream::app);
		err << "Error: hashmap folder doesn't exist\n";
		err.close();
	}else{
		fs::directory_iterator end_it;				//get tonberry/hashmap folder iterator
		for(fs::directory_iterator it(hashpath); it!=end_it; it++){
			if(fs::is_regular_file(it->status())){	//if we got a regular file
				if(it->path().extension() == ".csv"){	//we check its extension, if .csv file:

					ifstream hashfile;
					hashfile.open (it->path().string(), ifstream::in);	//open it and dump into the map
					string line;
					if (hashfile.is_open())
					{
						while ( getline(hashfile, line) ) //Omzy's original code
						{
							int comma = line.find(",");
							string field = line.substr(0, comma);
							string valuestr = line.substr(comma + 1, line.length()).c_str();
							uint64_t value = ToNumber<uint64_t>(valuestr);
							hashmap.insert(pair<uint64_t, string>(value, field)); //key, value for unique names, value, key for unique hashvals
						}
						hashfile.close();
					}

				}
			}
		}
	}
}

void loadcollfile ()	//Expects collisions.csv to be in ff8/tonberry directory
{
	ifstream collfile;
	collfile.open ("tonberry\\collisions.csv", ifstream::in);
	string line;
	if (collfile.is_open())
	{
		while ( getline(collfile, line) ) //~10000 total number of 128x256 texture blocks in ff8
		{
			int comma = line.find(",");
			string field = line.substr(0, comma);
			string valuestr = line.substr(comma + 1, line.length()).c_str();
			uint64_t value = ToNumber<uint64_t>(valuestr);
			collmap.insert(pair<uint64_t, string>(value, field)); //key, value for unique names, value, key for unique hashvals
		}
		collfile.close();
	}
}

void loadcoll2file ()	//Expects hash2map.csv to be in ff8/tonberry directory
{
	ifstream coll2file;
	coll2file.open ("tonberry\\hash2map.csv", ifstream::in);
	string line;
	if (coll2file.is_open())
	{
		while ( getline(coll2file, line) ) //~10000 total number of 128x256 texture blocks in ff8
		{
			int comma = line.find(",");
			string field = line.substr(0, comma);
			string valuestr = line.substr(comma + 1, line.length()).c_str();
			//BigInteger value = BigInteger(valuestr);
			//value = ToNumber<BigInt>(valuestr);
			coll2map.insert(pair<string, string>(valuestr, field)); //key, value for unique names, value, key for unique hashvals
		}
		coll2file.close();
	}
}

//Mod Jay
void loadobjfile ()	//Expects objmap.csv to be in ff8/tonberry directory
{
	fs::path hashpath("tonberry/objmap");
	if(!fs::exists(hashpath)){
		ofstream err;								//Error reporting file
		err.open("tonberry/error.txt", ofstream::out | ofstream::app);
		err << "Error: objmap folder doesn't exist\n";
		err.close();
	}else{
		fs::directory_iterator end_it;
		for(fs::directory_iterator it(hashpath); it!=end_it; it++){
			if(fs::is_regular_file(it->status())){	//if we got a regular file
				if(it->path().extension() == ".csv"){	//we check its extension, if .csv file:
				
					ifstream objfile;
					objfile.open (it->path().string(), ifstream::in);
					string line;
					if (objfile.is_open()){
						while ( getline(objfile, line) ){
							int comma = line.find(",");
							string obj = line.substr(0, comma);
							string valuestr = line.substr(comma + 1, line.length()).c_str();
							uint64_t value = ToNumber<uint64_t>(valuestr);
							objmap.insert(pair<uint64_t, string>(value, obj)); //key, value for unique names, value, key for unique hashvals
						}
						objfile.close();
					}

				}
			}
		}
	}
}

void GlobalContext::Init ()
{
	//------------
	/*_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
	HANDLE hLogFile;
	hLogFile = CreateFile("c:\\log.txt", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	_CrtSetReportFile( _CRT_ERROR, hLogFile );
	_RPT0(_CRT_ERROR,"file message\n");
	CloseHandle(hLogFile);*/
	//------------
    Graphics.Init();
	loadprefs();
	loadhashfile();
	loadcollfile();
	loadcoll2file();
	loadobjfile();
	initCache();//has to be called after loadprefs() so we get the propper cache_size!
}

void Hash_Algorithm_1 (BYTE* pData, UINT pitch, int width, int height)	//hash algorithm that preferences top and left sides
{
	int blocksize = 16;
	UINT x, y;
	int pix = 0;
	int toppix = 0;
	int botpix = 0;
	for (x = 0; x < 8; x++) //pixvals 0->31
	{
		for (y = 0; y < 4; y++)
		{
			if (x*blocksize < width && y*blocksize < height) //respect texture sizes
			{
				RGBColor* CurRow = (RGBColor*)(pData + (y*blocksize) * pitch);
				RGBColor Color = CurRow[x*blocksize];
				pixval[pix] = (Color.r + Color.g + Color.b) / 3;
				if (y*blocksize < 128) { if (toppix < 32) { objval[toppix] = pixval[pix]; toppix++; } }
				else if (botpix < 32) { objval[botpix + 32] = pixval[pix]; botpix++; }
			} else { pixval[pix] = 0; } //out of bounds
			pix++;
		}				
	}
	for (x = 0; x < 2; x++) //pixvals 32->55
	{
		for (y = 4; y < 16; y++)
		{
			if (x*blocksize < width && y*blocksize < height) //respect texture sizes
			{
				RGBColor* CurRow = (RGBColor*)(pData + (y*blocksize) * pitch);
				RGBColor Color = CurRow[x*blocksize];
				pixval[pix] = (Color.r + Color.g + Color.b) / 3;
				if (y*blocksize < 128) { if (toppix < 32) { objval[toppix] = pixval[pix]; toppix++; } }
				else if (botpix < 32) { objval[botpix + 32] = pixval[pix]; botpix++; }
			} else { pixval[pix] = 0; } //out of bounds
			pix++;
		}				
	}
	for (x = 3; x < 7; x+=3) //pixvals 56->63, note +=3
	{
		for (y = 5; y < 15; y+=3)	//note +=3
		{
			if (x*blocksize < width && y*blocksize < height) //respect texture sizes
			{
				RGBColor* CurRow = (RGBColor*)(pData + (y*blocksize) * pitch);
				RGBColor Color = CurRow[x*blocksize];
				pixval[pix] = (Color.r + Color.g + Color.b) / 3;
				if (y*blocksize < 128) { if (toppix < 32) { objval[toppix] = pixval[pix]; toppix++; } }
				else if (botpix < 32) { objval[botpix + 32] = pixval[pix]; botpix++; }
			} else { pixval[pix] = 0; } //out of bounds
			pix++;
		}				
	}
}

void Hash_Algorithm_2 (BYTE* pData, UINT pitch, int width, int height)	//hash algorithm that chooses unique pixels selected by hand
{
	int pix = 0;
	UINT x0, y0;
	UINT x[76] = {44, 0, 17, 25, 15, 111, 35, 25, 3, 46, 112, 34, 21, 1, 72, 80, 25, 32, 15, 4, 123, 16, 47, 14, 110, 78, 3, 66, 0, 86, 58, 27, 39, 4, 6, 49, 7, 71, 121, 17, 22, 16, 84, 115, 118, 119, 126, 59, 96, 88, 64, 1, 21, 31, 107, 92, 73, 116, 118, 58, 47, 18, 93, 78, 97, 106, 107, 77, 99, 13, 100, 125, 12, 33, 53, 61};
	UINT y[76] = {243, 0, 2, 19, 35, 24, 0, 12, 23, 7, 5, 0, 4, 0, 2, 218, 30, 2, 20, 23, 4, 4, 2, 8, 7, 7, 25, 0, 1, 0, 11, 15, 2, 0, 0, 1, 15, 15, 16, 7, 7, 0, 244, 245, 245, 245, 253, 203, 135, 184, 9, 15, 80, 81, 244, 245, 249, 255, 238, 237, 216, 218, 240, 216, 116, 164, 244, 247, 236, 245, 21, 59, 25, 8, 16, 108};
	for (int i = 0; i < 76; i++) //pixvals 0->75
	{
		if (x[i] < width && y[i] < height) //respect texture sizes
		{
			RGBColor* CurRow = (RGBColor*)(pData + (y[i]/*blocksize*/) * pitch); //blocksize already included
			RGBColor Color = CurRow[x[i]/*blocksize*/];
			pixval2[pix] = (Color.r + Color.g + Color.b) / 3;
		} else { pixval2[pix] = 0; } //out of bounds
		pix++;
	}

	for (x0 = 0; x0 < 44; x0+=4) //pixvals 76->97, note +=4
	{
		for (y0 = 7; y0 < 16; y0+=8) //note +=8
		{
			if (x0 < width && y0 < height) //respect texture sizes
			{
				RGBColor* CurRow = (RGBColor*)(pData + (y0/*blocksize*/) * pitch); //blocksize already included
				RGBColor Color = CurRow[x0/*blocksize*/];
				pixval2[pix] = (Color.r + Color.g + Color.b) / 3;
			} else { pixval2[pix] = 0; } //out of bounds
			pix++;
		}				
	}
}

string getsysfld (BYTE* pData, UINT pitch, int width, int height, string sysfld)	//Exception method for sysfld00 and sysfld01
{
	UINT x = 177;
	UINT y = 155;
	RGBColor* CurRow = (RGBColor*)(pData + (y) * pitch);
	RGBColor Color = CurRow[x];
	int sysval = (Color.r + Color.g + Color.b) / 3;
	string syspage = "13";
	switch (sysval) {
		case 43: syspage = "13"; break;
		case 153: syspage = "14"; break;
		case 150: syspage = "15"; break;
		case 101: syspage = "16"; break;
		case 85: syspage = "17"; break;
		case 174: syspage = "18"; break;
		case 170: syspage = "19"; break;
		case 255: syspage = "20"; break;
		default: syspage = "13"; break;
	}
	return sysfld.substr(0, 9) + syspage;
}

string geticonfl (BYTE* pData, UINT pitch, int width, int height, string iconfl)	//Exception method for iconfl00, iconfl01, iconfl02, iconfl03, iconflmaster
{
	UINT x = 0;
	UINT y = 0;
	if (iconfl == "iconfl00_13") { x = 82; y = 150; }
	else if (iconfl == "iconfl01_13") { x = 175; y = 208; }
	else if (iconfl == "iconfl02_13") { x = 216; y = 108; }
	else if (iconfl == "iconfl03_13") { x = 58; y = 76; }
	else if (iconfl == "iconflmaster_13") { x = 215; y = 103; }

	RGBColor* CurRow = (RGBColor*)(pData + (y) * pitch);
	RGBColor Color = CurRow[x];
	int colR = Color.r;
	int colG = Color.g;
	int colB = Color.b;
	if (colR == 0) { colR++; }
	if (colG == 0) { colG++; }
	if (colB == 0) { colB++; }
	int icval = colR * colG * colB;

	string icpage = "13";
	switch (icval) {
		case 65025: icpage = "13"; break;
		case 605160: icpage = "14"; break;
		case 1191016: icpage = "15"; break;
		case 189: icpage = "16"; break;
		case 473304: icpage = "17"; break;
		case 20992: icpage = "18"; break;
		case 859625: icpage = "19"; break;
		case 551368: icpage = "20"; break;
		case 1393200: icpage = "21"; break;
		case 931500: icpage = "22"; break;
		case 1011240: icpage = "23"; break;
		case 1395640: icpage = "24"; break;
		case 1018024: icpage = "25"; break;
		case 411864: icpage = "26"; break;
		case 80064: icpage = "27"; break;
		case 4410944: icpage = "28"; break;
		default: icpage = "13"; break;
	}
	return iconfl.substr(0, iconfl.size()-2) + icpage;
}

string getobj (uint64_t & hash) //if previously unmatched, searches through object map for objects in top left/bottom left memory quarters, finally NO_MATCH is returned
{
	objtop = 0;
	objbot = 0;
	int lastpixel = objval[63];
    for (int i = 0; i < 64; i++)
    {
		if (i < 32) { objtop *= 2; }
		else { objbot *= 2; }
		if ((objval[i] - lastpixel) >= 0)
		{
			if (i < 32) { objtop++; }
			else { objbot++; }
		}
		lastpixel = objval[i];
    }
	it = objmap.find(objtop);
	if (it != objmap.end()) { 
		hash = objtop;
		return it->second; }
	it = objmap.find(objbot);
	if (it != objmap.end()) { 
		hash = objbot;
		return it->second; }
	hash = 0;
	return "NO_MATCH";
}

string getfield (uint64_t & hash)	//simple sequential bit comparisons
{
	ofstream checkfile;
	//checkfile.open("tonberry/tests/hashcache_test.txt", ofstream::out| ofstream::app);
	hashval = 0;
	int lastpixel = pixval[63];
    for (int i = 0; i < 64; i++)
    {
        hashval *= 2;
		if ((pixval[i] - lastpixel) >= 0) {	hashval++; }
		lastpixel = pixval[i];
    }

	it = hashmap.find(hashval);
	if (it != hashmap.end()) { 
		hash = hashval;
		return it->second; 
	}
	else {
		it = collmap.find(hashval);
		if (it != collmap.end()) {
			hash = hashval;
			return "COLLISION"; 
		}
	}

	return getobj(hash);
}

string getfield2 ()	//simple sequential bit comparisons, algorithm 2
{
	hashval2 = 0;
	int lastpixel = pixval2[97];
    for (int i = 0; i < 98; i++)
    {
        hashval2 *= 2;
		if ((pixval2[i] - lastpixel) >= 0) {	hashval2 += 1; }
		lastpixel = pixval2[i];
    }
	it2 = coll2map.find(hashval2.getNumber());
	if (it2 != coll2map.end()) { return it2->second; }
	return "NO_MATCH2";
}

uint64_t parseiconfl(string texname){ 
//that crappy quick-fix function will allow us to identify the ic textures until the new hashing is ready.
	uint64_t hash = 0;
	string token;
	ofstream check;

	token = texname.substr(6, 2 );
	if(token == "ma"){//iconflmaster_XX
		hash += 11111111111111100000;	
	}
	else if(token == "00"){
		hash += 2222222220000000000;
	}
	else if(token == "01"){
		hash += 3333333330000000000;
	}
	else if(token == "02"){
		hash += 4444444440000000000;
	}
	else if(token == "03"){
		hash += 5555555550000000000;
	}

	token = texname.substr( texname.find("_")+1 );
	
	if(token == "13") hash += 13;
	else if(token == "14") hash += 14;
	else if(token == "15") hash += 99;//aqui
	else if(token == "16") hash += 16;
	else if(token == "17") hash += 17;
	else if(token == "18") hash += 18;
	else if(token == "19") hash += 19;
	else if(token == "20") hash += 20;
	else if(token == "21") hash += 21;
	else if(token == "22") hash += 22;
	else if(token == "23") hash += 23;
	else if(token == "24") hash += 24;
	else if(token == "25") hash += 25;
	else if(token == "26") hash += 26;
	else if(token == "27") hash += 27;
	else if(token == "28") hash += 28;

	return hash;
}

uint64_t parsesysfld(string texname){
	uint64_t hash = 0;
	string token;
	ofstream check;

	token = texname.substr(6, 2 );
	if(token == "00"){
		hash += 6666666660000000000;
	}
	else if(token == "01"){
		hash += 7777777770000000000;
	}
	token = texname.substr( texname.find("_")+1 );

	if(token == "13") hash += 13;
	else if(token == "14") hash += 14;
	else if(token == "15") hash += 15;//aqui
	else if(token == "16") hash += 16;
	else if(token == "17") hash += 17;
	else if(token == "18") hash += 18;
	else if(token == "19") hash += 19;
	else if(token == "20") hash += 20;
	else if(token == "21") hash += 21;
	else if(token == "22") hash += 22;
	else if(token == "23") hash += 23;
	else if(token == "24") hash += 24;
	else if(token == "25") hash += 25;
	else if(token == "26") hash += 26;
	else if(token == "27") hash += 27;
	else if(token == "28") hash += 28;

	return hash;
}

int m;

//Final unlockrect
void GlobalContext::UnlockRect (D3DSURFACE_DESC &Desc, Bitmap &BmpUseless, HANDLE Handle) //note BmpUseless
{
    IDirect3DTexture9* pTexture = (IDirect3DTexture9*)Handle;   

    String debugtype = String("");

	
      //  ofstream checkfile;
      //  checkfile.open ("tonberry/tests/checkiconflmaster.csv", ofstream::out | ofstream::app);
    if (pTexture && Desc.Width < 640 && Desc.Height < 480 && Desc.Format == D3DFORMAT::D3DFMT_A8R8G8B8 && Desc.Pool == D3DPOOL::D3DPOOL_MANAGED)    //640x480 are video
    {
        D3DLOCKED_RECT Rect;
        pTexture->LockRect(0, &Rect, NULL, 0);
        UINT pitch = (UINT)Rect.Pitch;
        BYTE* pData = (BYTE*)Rect.pBits;

        uint64_t hash;
        Hash_Algorithm_1(pData, pitch, Desc.Width, Desc.Height);    //Run Hash_Algorithm_1
        string texturename = getfield(hash);

        if (texturename == "sysfld00_13" || texturename == "sysfld01_13") { texturename = getsysfld(pData, pitch, Desc.Width, Desc.Height, texturename);hash = parsesysfld(texturename);} //Exception for sysfld00 and sysfld01
        if (texturename == "iconfl00_13" || texturename == "iconfl01_13" || texturename == "iconfl02_13" || texturename == "iconfl03_13" || texturename == "iconflmaster_13") { texturename = geticonfl(pData, pitch, Desc.Width, Desc.Height, texturename); hash = parseiconfl(texturename);} //Exception for iconfl00, iconfl01, iconfl02, iconfl03, iconflmaster
        
        if (texturename == "NO_MATCH")
        { //Handle inválido, lo borro, pero no su posible textura asociada.
			texcache->erase(Handle);
            debugtype = String("nomatch");
        } else { //Texture FOUND in Hash_Algorithm_1 OR is a COLLISION
            if (texturename == "COLLISION")
            { //Run Hash_Algorithm_2
                Hash_Algorithm_2(pData, pitch, Desc.Width, Desc.Height);
                texturename = getfield2();
                if (texturename == "NO_MATCH2") { 
					//checkfile<<"\nCache erased on nomatch2, size: " << texcache->entries_ << endl;
					texcache->erase(Handle);
					debugtype = String("nomatch2"); 
				}
            }

            string filename = texdir + "textures\\" + texturename.substr(0, 2) + "\\" + texturename.substr(0, texturename.rfind("_")) + "\\" + texturename + ".png";
			if(!texcache->update(Handle,hash)){//directly updated if it succeeds we just end unlockrect cycle.
                ifstream ifile(filename);
                if (ifile.fail()) { 
					texcache->erase(Handle);
					debugtype = String("noreplace"); //No file, allow normal SetTexture
                } else {    //Load texture into cache
                    LPDIRECT3DDEVICE9 Device = g_Context->Graphics.Device();
                    IDirect3DTexture9* newtexture;  
                    Bitmap Bmp;
                    Bmp.LoadPNG(String(filename.c_str()));
                    DWORD Usage = D3DUSAGE_AUTOGENMIPMAP;
                    D3DPOOL Pool = D3DPOOL_MANAGED;
                    D3DFORMAT Format = D3DFMT_A8R8G8B8;
                    Device->CreateTexture(int(resize_factor*(float)Desc.Width), int(resize_factor*(float)Desc.Height), 0, Usage, Format, Pool, &newtexture, NULL);
                    D3DLOCKED_RECT newRect;
                    newtexture->LockRect(0, &newRect, NULL, 0);
                    BYTE* newData = (BYTE *)newRect.pBits;
                    for(UINT y = 0; y < Bmp.Height(); y++)
                    {
                        RGBColor* CurRow = (RGBColor *)(newData + y * newRect.Pitch);
                        for(UINT x = 0; x < Bmp.Width(); x++)   //works for textures of any size (e.g. 4-bit indexed)
                        {
                            RGBColor Color = Bmp[Bmp.Height() - y - 1][x];  //must flip image
                            CurRow[x] = RGBColor(Color.b, Color.g, Color.r, Color.a);
                        }
                    }
                    newtexture->UnlockRect(0); //Texture loaded
                    HANDLE tempnewhandle = (HANDLE)newtexture;

					texcache->insert(Handle,hash,tempnewhandle);
               }

            }

        }
        pTexture->UnlockRect(0); //Finished reading pTextures bits
    } else { //Video textures/improper format
       // this is the beauty of your solution; you replaced that whole O(n^2) loop bullshit with one line ;)
		texcache->erase(Handle);
		debugtype = String("error");
    }
    //Debug
    if(debugmode){
        String debugfile = String("tonberry\\debug\\") + debugtype + String("\\") + String::ZeroPad(String(m), 3) + String(".bmp");
        D3DXSaveTextureToFile(debugfile.CString(), D3DXIFF_BMP, pTexture, NULL);
        m++; //debug
    }
}

//Final settex

bool GlobalContext::SetTexture(DWORD Stage, HANDLE* SurfaceHandles, UINT SurfaceHandleCount)
{
#if DEBUG
	StartCounter();
#endif
       for (int j = 0; j < SurfaceHandleCount; j++) {
               IDirect3DTexture9* newtexture;
               if (SurfaceHandles[j] && (newtexture = (IDirect3DTexture9*)texcache->at(SurfaceHandles[j]))) {
				  
                       g_Context->Graphics.Device()->SetTexture(Stage, newtexture);
                   //texcache->fastupdate(SurfaceHandles[j]);
					   //((IDirect3DTexture9*)SurfaceHandles[j])->Release();
#if DEBUG
  debug << GetCounter() << endl;
#endif
                       return true;
               } // Texture replaced!
       }
#if DEBUG
  debug << GetCounter() << endl;
#endif
       return false;
}

//Unused functions
void GlobalContext::UpdateSurface(D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle) {}
void GlobalContext::Destroy(HANDLE Handle) {}
void GlobalContext::CreateTexture (D3DSURFACE_DESC &Desc, Bitmap &Bmp, HANDLE Handle, IDirect3DTexture9** ppTexture) {}
void GlobalContext::BeginScene () {}