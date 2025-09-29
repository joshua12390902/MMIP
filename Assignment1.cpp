#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <algorithm>
#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
using namespace std;
namespace fs = std::filesystem;

struct GrayImage{
    int w{0};
    int h{0};
    vector<uint8_t> pix; // row-major，大小 = w*h uint8_t = 0-255 正好存放灰階pixel
    uint8_t& at(int x, int y) { return pix[y*w + x]; }
    const uint8_t& at(int x, int y) const { return pix[y*w + x]; }
};

//(a)Image reading
bool ensure_dir(const std::string& path) {
#ifdef _WIN32
    int rc = mkdir(path.c_str());  // Windows 不接受權限參數
#else
    int rc = mkdir(path.c_str(), 0755);  // Linux / macOS
#endif
    return rc == 0 || errno == EEXIST;
}


bool read_raw(const string& path, GrayImage& img){
    FILE* f = fopen(path.c_str(), "rb"); //讀binary
    const int W=512, H=512, N=W*H;
    if (!f) { 
        perror(("open " + path).c_str()); 
        return false; 
    }
    img.w = W; img.h = H; 
    img.pix.resize(N); //分配大小為N的heap
    size_t got = fread(img.pix.data(), 1, N, f); //讀N個byte到img.pix
    fclose(f);
    if (got != N) { 
        cerr << "file size error: " << got << "vs." << N << endl;              
        return false; 
    }
    return true;
}

// unsigned char *stbi_load(
//     char const *filename,   // [in]  檔名
//     int *x,                 // [out] 讀到的寬度
//     int *y,                 // [out] 讀到的高度
//     int *channels_in_file,  // [out] 原圖實際有幾個通道
//     int desired_channels    // [in]  要求輸出幾個通道
// );

bool read_gray_any(const string& path, GrayImage& img){
    int comp,w,h;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, STBI_grey); //stbi轉灰階像素陣列，用char不用int => int浪費空間
    if (!data) {
        cerr << "stbi_load " << path << " failed" << endl;
        return false;
    }
    img.w = w; img.h = h;
    img.pix.assign(data, data + img.w * img.h); //從 data 指標開始到 data + img.w * img.h 結束，複製這段記憶體內容到 img.pix。
    stbi_image_free(data); 
    return true;
}

bool write_pgm(const string& path, const GrayImage& img) {
    ofstream f(path, ios::binary);  //輸出檔案binary
    if (!f) { 
        cerr << "cannot write: " << path << "\n"; 
        return false; 
    }
    f << "P5\n" << img.w << " " << img.h << "\n255\n";
    f.write((const char*)img.pix.data(), img.w * img.h);
    return (bool)f;
}

void print_center10(const GrayImage& img,const string& tag){
    for (int y = img.h / 2 - 5; y < img.h / 2 + 5; y++){
        for (int x = img.w / 2 - 5; x < img.w / 2 + 5; x++){
            cout << setw(3) << (int)img.at(x,y) << (x == img.w/2+4 ? '\n' : ' '); //setw(3)設定每個值都格式化為 3 字元寬度，(int)把uint8_t轉成int輸出
        }
        cout << endl;
    }

    ofstream csv(string("results/") + tag + "_center10.csv");

    for (int y = img.h / 2 - 5; y < img.h / 2 + 5; y++) {
        for (int x = img.w / 2 - 5; x < img.w / 2 + 5; x++) {
            csv << (int)img.at(x,y) << (x == img.w/2+4  ? '\n' : ',');
        }
    }
}

//(b)Image enhancement toolkit
GrayImage negative(GrayImage& in) {
    GrayImage out = in;
    for (auto& p : out.pix) {
        p = 255 - p; //反相
    }
    return out;
}

GrayImage log_transform(const GrayImage& in){
    GrayImage out = in;
    double c = 255.0 / log(256.0); 
    for (auto& p : out.pix) {
        p = static_cast<uint8_t>(c * log(1 + p)); //結果可能超出 0–255，或不是整數。常用 static_cast<uint8_t> 把它轉回 0–255 的整數型態
    }
    return out;
}

GrayImage gamma_transform(const GrayImage& in, double gamma){
    GrayImage out = in;
    for (auto& p : out.pix) {
        p = static_cast<uint8_t>(255.0 * pow(p / 255.0, gamma)); 
    }
    return out;
}

//(c)Image downsampling and upsampling
static inline double map_coord(int x, int src_len, int dst_len) { //新圖算第 N 個像素，map_coord 得到它對應到原圖裡哪個位置，才能決定要取哪個原圖的像素
    return ((x + 0.5) * (double)src_len / (double)dst_len) - 0.5;
}

GrayImage resize_nearest(const GrayImage& in, int newW, int newH){
    GrayImage out;
    out.w = newW; out.h = newH;
    out.pix.resize(newW * newH);

    for (int y = 0; y < newH; ++y) {
        int sy = static_cast<int>(lround(map_coord(y, in.h, newH))); //四捨五入
        if (sy < 0) sy = 0;
        if (sy >= in.h) sy = in.h - 1;
        for (int x = 0; x < newW; ++x) {
            int sx = static_cast<int>(lround(map_coord(x, in.w, newW)));
            if (sx < 0) sx = 0;
            if (sx >= in.w) sx = in.w - 1;
            out.at(x, y) = in.at(sx, sy);
        }
    }
    return out;
}

GrayImage resize_bilinear(const GrayImage& in, int newW, int newH){
    GrayImage out;
    out.w = newW; out.h = newH;
    out.pix.resize(newW * newH);

    for (int y = 0; y < newH; ++y) {
        double sy = map_coord(y, in.h, newH);
        int sy0 = static_cast<int>(floor(sy));
        int sy1 = sy0 + 1;
        double dy = sy - sy0;
        if (sy0 < 0) { sy0 = 0; dy = 0.0; }
        if (sy1 < 0) { sy1 = 0; }
        if (sy0 >= in.h) { sy0 = in.h - 1; dy = 0.0; }
        if (sy1 >= in.h) { sy1 = in.h - 1; }

        for (int x = 0; x < newW; ++x) {
            double sx = map_coord(x, in.w, newW);
            int sx0 = static_cast<int>(floor(sx));
            int sx1 = sx0 + 1;
            double dx = sx - sx0;
            if (sx0 < 0) { sx0 = 0; dx = 0.0; }
            if (sx1 < 0) { sx1 = 0; }
            if (sx0 >= in.w) { sx0 = in.w - 1; dx = 0.0; }
            if (sx1 >= in.w) { sx1 = in.w - 1; }

            //雙線性插值
            double p00 = in.at(sx0, sy0);
            double p10 = in.at(sx1, sy0);
            double p01 = in.at(sx0, sy1);
            double p11 = in.at(sx1, sy1);
            double pxy = p00 * (1 - dx) * (1 - dy) +
                         p10 * dx * (1 - dy) +
                         p01 * (1 - dx) * dy +
                         p11 * dx * dy;
            out.at(x, y) = static_cast<uint8_t>(lround(pxy));
        }
    }
    return out;
}

// 建立一個 SDL3 Texture，將 GrayImage (8-bit 灰階) 擴成 24-bit RGB 再丟進去
static SDL_Texture* make_texture(SDL_Renderer* R, const GrayImage& g) {
    // 用 RGB888（每像素 4 bytes，安全且相容）
    SDL_Texture* tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_XRGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     g.w, g.h);

    if (!tex) {
        cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        return nullptr;
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch)) { // SDL3: 回傳 bool
        cerr << "SDL_LockTexture failed: " << SDL_GetError() << "\n";
        SDL_DestroyTexture(tex);
        return nullptr;
    }

    // 將灰階 v 複製到 R/G/B 三通道：0x00RRGGBB
    for (int y = 0; y < g.h; ++y) {
        uint32_t* row = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(pixels) + y * pitch
        );
        for (int x = 0; x < g.w; ++x) {
            uint8_t v = g.at(x, y);
            uint32_t rgb = (uint32_t(v) << 16) | (uint32_t(v) << 8) | uint32_t(v);
            row[x] = rgb;
        }
    }

    SDL_UnlockTexture(tex);

    // 讓縮放時更平滑；若想看馬賽克，可改 SDL_SCALEMODE_NEAREST
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    return tex;
}

int main() {
    ensure_dir("results");

    vector<fs::path> raw_paths, img_paths;

    // 掃描 ./data
    for (const auto& entry : fs::directory_iterator("data")) { 
        if (!entry.is_regular_file()) continue;
        fs::path p = entry.path();
        string ext = p.extension().string(); //副檔名
        for (auto& c : ext) c = tolower(c);

        if (ext == ".raw") raw_paths.push_back(p);
        else if (ext == ".bmp" || ext == ".jpg" || ext == ".jpeg") img_paths.push_back(p);
    }

    if (raw_paths.size() != 3 || img_paths.size() != 3) {
        cerr << "data folder must contain exactly 3 RAW and 3 BMP/JPG.\n";
        cerr << "Found RAW=" << raw_paths.size() << " IMG=" << img_paths.size() << endl;
        return 1;
    }

    // 排序，確保順序穩定
    auto by_name = [](const fs::path& a, const fs::path& b) {
        return a.filename().string() < b.filename().string();
    };
    sort(raw_paths.begin(), raw_paths.end(), by_name);
    sort(img_paths.begin(), img_paths.end(), by_name);

    vector<GrayImage> imgs;
    vector<string> tags; // 用來記錄輸出的檔名，不含副檔名
    imgs.reserve(6);

    // 讀 RAW
    for (auto& p : raw_paths) {
        GrayImage g;
        if (!read_raw(p.string(), g)) return 2;
        imgs.push_back(g);
        tags.push_back(p.stem().string()); 
    }

    // 讀 BMP/JPG
    for (auto& p : img_paths) {
        GrayImage g;
        if (!read_gray_any(p.string(), g)) return 3;
        imgs.push_back(g);
        tags.push_back(p.stem().string()); 
    }

    // 輸出
    double g = 2.2; //gamma值
    for (int i = 0; i < 6; ++i) {
        string tag = tags[i];
        print_center10(imgs[i], tag);
        write_pgm("results/" + tag + ".pgm", imgs[i]);

        auto neg = negative(imgs[i]);
        write_pgm("results/" + tag + "_neg.pgm", neg);

        // Log
        auto logimg = log_transform(imgs[i]);
        write_pgm("results/" + tag + "_log.pgm", logimg);

        // Gamma
        auto gim = gamma_transform(imgs[i], g);
        write_pgm("results/" + tag + "_gamma" + ".pgm", gim);
    }
    // c) Down/Up sampling comparisons
    for (int i = 0; i < (int)imgs.size(); ++i) {
        const string& tag = tags[i];
        const GrayImage& g = imgs[i];

        // 我們以「若當前影像不是 512×512，先雙線性重取樣到 512×512」確保規格一致（尤其是 BMP/JPG 不是 512）
        GrayImage base = (g.w == 512 && g.h == 512) ? g : resize_bilinear(g, 512, 512);

        // (i) 512->128
        auto n_512_128 = resize_nearest(base, 128, 128);
        auto b_512_128 = resize_bilinear(base, 128, 128);
        write_pgm("results/" + tag + "_n_512to128.pgm", n_512_128);
        write_pgm("results/" + tag + "_b_512to128.pgm", b_512_128);

        // (ii) 512->32
        auto n_512_32 = resize_nearest(base, 32, 32);
        auto b_512_32 = resize_bilinear(base, 32, 32);
        write_pgm("results/" + tag + "_n_512to32.pgm", n_512_32);
        write_pgm("results/" + tag + "_b_512to32.pgm", b_512_32);

        // (iii) 32->512 （先下，再上）
        auto n_32 = resize_nearest(base, 32, 32);
        auto b_32 = resize_bilinear(base, 32, 32);
        auto n_32_512 = resize_nearest(n_32, 512, 512);
        auto b_32_512 = resize_bilinear(b_32, 512, 512);
        write_pgm("results/" + tag + "_n_32to512.pgm", n_32_512);
        write_pgm("results/" + tag + "_b_32to512.pgm", b_32_512);

        // (iv) 512->1024x512（水平放大 2x）
        auto n_1024_512 = resize_nearest(base, 1024, 512);
        auto b_1024_512 = resize_bilinear(base, 1024, 512);
        write_pgm("results/" + tag + "_n_512to1024x512.pgm", n_1024_512);
        write_pgm("results/" + tag + "_b_512to1024x512.pgm", b_1024_512);

        // (v) 128x128->256x512（先等比下到 128x128，再非等比上到 256x512）
        auto n_128 = resize_nearest(base, 128, 128);
        auto b_128 = resize_bilinear(base, 128, 128);
        auto n_128_256x512 = resize_nearest(n_128, 256, 512);
        auto b_128_256x512 = resize_bilinear(b_128, 256, 512);
        write_pgm("results/" + tag + "_n_128to256x512.pgm", n_128_256x512);
        write_pgm("results/" + tag + "_b_128to256x512.pgm", b_128_256x512);
    }


    cout << "[DONE] Saved outputs in ./results (filenames match originals)\n";
    
        // === 顯示在螢幕上（SDL3） ===
    // 初始化（SDL3 的 SDL_Init 回傳 bool）
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        // 這裡不直接結束：PGM/CSV 已輸出，還是算完成部分要求
    } else {
        // 2×3 拼貼，每格最大顯示大小可自行調
        const int cols = 3, rows = 2;
        const int cellW = 512, cellH = 512;
        const int winW = cols * cellW, winH = rows * cellH;

        SDL_Window*   W = SDL_CreateWindow("MMIP Viewer", winW, winH, SDL_WINDOW_RESIZABLE);
        if (!W) {
            cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
            SDL_Quit();
        } else {
            // SDL3：第二參數給 nullptr 讓它選預設 renderer
            SDL_Renderer* R = SDL_CreateRenderer(W, nullptr);
            if (!R) {
                cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
                SDL_DestroyWindow(W);
                SDL_Quit();
            } else {
                // 建立 6 張 texture
                vector<SDL_Texture*> tex(6, nullptr);
                for (int i = 0; i < 6; ++i) {
                    tex[i] = make_texture(R, imgs[i]);
                }

                bool running = true;
                SDL_Event e;
                while (running) {
                    while (SDL_PollEvent(&e)) {
                        if (e.type == SDL_EVENT_QUIT) running = false;
                        if (e.type == SDL_EVENT_KEY_DOWN) {
                            
                            if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_Q) running = false;
                        }
                    }

                    // 背景深灰
                    SDL_SetRenderDrawColor(R, 20, 20, 20, 255);
                    SDL_RenderClear(R);

                    // 逐格畫上 6 張圖
                    for (int i = 0; i < 6; ++i) {
                        if (!tex[i]) continue;
                        int r = i / 3, c = i % 3;

                        // 該 cell 的區域
                        float cx = float(c * cellW), cy = float(r * cellH);
                        float cw = float(cellW),     ch = float(cellH);

                        // 讀取 texture 原尺寸（SDL3: 回傳 float）
                        float twf = 0.f, thf = 0.f;
                        SDL_GetTextureSize(tex[i], &twf, &thf);

                        // 等比例縮放置中
                        float sx = cw / twf, sy = ch / thf;
                        float s  = (sx < sy ? sx : sy);
                        float dw = twf * s,  dh = thf * s;
                        SDL_FRect dst{ cx + (cw - dw) * 0.5f, cy + (ch - dh) * 0.5f, dw, dh };

                        // SDL3：用 SDL_RenderTexture（取代 SDL2 的 SDL_RenderCopy）
                        SDL_RenderTexture(R, tex[i], nullptr, &dst);
                    }

                    SDL_RenderPresent(R);
                }

                for (auto* t : tex) if (t) SDL_DestroyTexture(t);
                SDL_DestroyRenderer(R);
                SDL_DestroyWindow(W);
                SDL_Quit();
            }
        }
    }

    return 0;
}
