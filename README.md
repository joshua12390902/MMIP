# README – MMIP Assignment 1

## 1. Requirements
Before compiling the code, make sure the following are installed:

- **C++ Compiler**
  - Windows: `g++` (MinGW-w64) or MSVC
  - Linux/Mac: `g++` with C++17 or later

- **Libraries**
  - [stb_image.h](https://github.com/nothings/stb) (already included in this repo, no extra install needed)
  - [SDL3](https://github.com/libsdl-org/SDL) (for displaying images on screen)

---

## 2. Installing SDL3

### Windows
1. Download SDL3 development package (VC version) from:  
   <https://github.com/libsdl-org/SDL/releases>  
   Example: `SDL3-devel-3.2.22-VC.zip`

2. Extract the folder, e.g.:  
   ```
   C:\Users\YourName\Downloads\SDL3-devel-3.2.22-VC\SDL3-3.2.22
   ```

3. update the `include` and `lib/x64` paths in **tasks.json** (already configured if you use the provided `.vscode` folder , just need to update the path).

4. update the `includePath` paths in **c_cpp_properties.json** (eg."C:/Users/penyi lee/Downloads/SDL3-devel-3.2.22-VC/SDL3-3.2.22/include")

5. Copy `SDL3.dll` (from `lib/x64`) into the same folder as the compiled `Assignment1.exe`.(already configured if you use the provided `MMIP` folder )

### Linux / WSL
```bash
sudo apt update
sudo apt install libsdl3-dev
```

### macOS
```bash
brew install sdl3
```

---

## 3. Data Preparation

- Place **six input images** in the `data/` folder:  (already configured if you use the provided `MMIP` folder )
  - `lena.raw`, `goldhill.raw`, `peppers.raw`
  - `baboon.bmp`, `boat.bmp`, `f16.bmp`

⚠️ Note:
- RAW files are assumed to be **512x512 grayscale, row-major order**.
- BMP/JPG will be converted to grayscale automatically.

---

## 4. Build Instructions

### VS Code (recommended)
- Press **F5** to build & run directly (tasks.json and launch.json are already set up).

### Manual (Windows example)
```powershell
g++ Assignment1.cpp -o Assignment1.exe ^
  -IC:/Users/YourName/Downloads/SDL3-devel-3.2.22-VC/SDL3-3.2.22/include ^
  -LC:/Users/YourName/Downloads/SDL3-devel-3.2.22-VC/SDL3-3.2.22/lib/x64 -lSDL3
```

### Linux / macOS
```bash
g++ Assignment1.cpp -o Assignment1 -lSDL3 -std=c++17
```

---

## 5. Running
```bash
./Assignment1
```
- Results will be saved in the `results/` folder as `.pgm` files.
- Central `10x10` pixel values are exported to `.csv` for each image.
- A window will open showing all six original images (SDL3).

---

## 6. Output Files
- **Results Folder (`results/`)**
  - Original grayscale images (`.pgm`)
  - Enhanced images: `*_neg.pgm`, `*_log.pgm`, `*_gamma.pgm`
  - Resized images for five cases (nearest and bilinear)
  - CSV files: `*_center10.csv`

---

## 7. Known Issues
- On Windows, if SDL3 window does not open:
  - Check that `SDL3.dll` is next to `Assignment1.exe`.
- On Linux/macOS, ensure `libsdl3-dev` is installed.
- Only `.raw`, `.bmp`, `.jpg`, `.jpeg` formats are supported.
