#include <iostream>
#include <unordered_map>
#include <windows.h>
#include <shlobj.h>
#include <sstream>
#include <vector>
#include <filesystem>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <format>
#include <fstream>


using namespace cv;
using std::string;
namespace fs = std::filesystem;


void core();
void coutPlus(const string& text, const string& color="DEFAULT", bool endLine=true);
void cerrPlus(const string& text);
string getColorEscapeSequence(const string& color);
void setConsoleTextColor(int colorCode);
string selectFolder();
void createBackup(const string& dir);
std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const string& imagePath, const Mat& image, int maxImageLength);
void processDirectory(const string& directoryPath, int maxImageLength);
void processTask(const string& imagePath, int maxImageLength, int taskId);
void runTasks(const std::vector<string>& imagePaths, int maxImageLength);
std::optional<Mat> readImage(const string& imagePath);
void saveImage(const string& outputFileName, const Mat& image, const std::vector<int>& compressionParams={});
std::vector<int> getCompressionParamsForImage(const string& imageType, int imageQuality=100);
string getRelativePath(const string& initialPath, const string& filePath);
int getFileSize(const string& filePath);
double getFileSizeInKB(const string& filePath);
void coutPlusPlus(const std::vector<std::string>& texts, const std::vector<std::string>& colors, bool endLine=true);


std::mutex mtx;
std::condition_variable cvThreads;
int activeThreads = 0;



int main() {
    core();
    return 0;
}


void core() {
    const string DIRECTORY = selectFolder();
    int maxImageLength;
    coutPlus("Enter the maximum length of the image's dimensions [px]:\n>> ", "blue", false);
    std::cin >> maxImageLength;

    createBackup(DIRECTORY);

    unsigned int numThreads = std::thread::hardware_concurrency();
    coutPlus("Number of threads supported: " + std::to_string(numThreads), "yellow");

    processDirectory(DIRECTORY, maxImageLength);

    // Wait until all tasks are completed
    std::unique_lock<std::mutex> lock(mtx);
    cvThreads.wait(lock, [] { return activeThreads == 0; });


    // TODO print statistics (how many images edited, how much space saved, how much space saved per image on average,
    //  how much time it took for all images, how much time it took per image, etc...

    system("pause");
}


void processTask(const string& imagePath, int maxImageLength, int taskId) {
    double oldImageSize = getFileSizeInKB(imagePath);
    oldImageSize = std::round(oldImageSize * 100) / 100;  // Round to two decimal places
//    {
//        std::unique_lock<std::mutex> lock(mtx);
//        std::cout << "Processing Task " << taskId << " on Thread " << std::this_thread::get_id() << std::endl;
//    }
//    std::this_thread::sleep_for(std::chrono::seconds(1));  // useless delay?

    std::optional<Mat> readResult = readImage(imagePath);
    if (!readResult.has_value()) {
        return;
    }
    Mat img = readResult.value();

    auto resizeResult = resizeImage(imagePath, img, maxImageLength);
    if (get<0>(resizeResult).has_value()) {
        img = get<0>(resizeResult).value();
    }

    // TODO user should be able to choose at start whether to convert all images to .jpg or keep them as same types
    std::vector<int> compressionParams = getCompressionParamsForImage("jpg", 50);

    saveImage(imagePath, img, compressionParams);


    int oldWidth = get<1>(resizeResult);
    int oldHeight = get<2>(resizeResult);
    int newWidth = get<3>(resizeResult);
    int newHeight = get<4>(resizeResult);

    double newImageSize = getFileSizeInKB(imagePath);
    newImageSize = std::round(newImageSize * 100) / 100;  // Round to two decimal places

    string msg = std::format("{}", imagePath);
    std::vector<string> texts;
    std::vector<std::string> colors;
    string fileSizeDiff = std::format("{}kB => {}kB", oldImageSize, newImageSize);
    string oldDimensions = std::format("[{}x{}]", oldWidth, oldHeight);
    string newDimensions = std::format("[{}x{}]", newWidth, newHeight);

    if (newWidth == -1) {
        string notDownscaled = std::format("{} (not downscaled)", oldDimensions);
        texts = {msg, ": ", notDownscaled, "; ", fileSizeDiff};
        colors = {"light-blue", "light-blue", "blue2", "light-blue", "yellow2"};
    } else {
        string downscaled = std::format("{} => {}", oldDimensions, newDimensions);
        texts = {msg, ": ", downscaled, "; ", fileSizeDiff};
        colors = {"green", "green", "blue2", "green", "yellow2"};
    }
    coutPlusPlus(texts, colors);

    {
        std::lock_guard<std::mutex> lock(mtx);
        activeThreads--;
    }

    cvThreads.notify_all();
}


int getFileSize(const string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file)
        return -1; // Error opening the file

    std::streampos fileSize = file.tellg();
    file.close();
    return fileSize;
}


double getFileSizeInKB(const string& filePath) {
    std::streampos fileSize = getFileSize(filePath);
    if (fileSize == -1) {
        cerrPlus("Failed to get image file size.");
    }
    double sizeKB = fileSize / 1024.0;

    return sizeKB;
}


void runTasks(const std::vector<string>& imagePaths, int maxImageLength) {
    unsigned int numTasks = imagePaths.size();
    for (int i = 0; i < numTasks; ++i) {
        // Wait until a thread is available
        std::unique_lock<std::mutex> lock(mtx);
        cvThreads.wait(lock, [] { return activeThreads < std::thread::hardware_concurrency(); });

        activeThreads++;

        std::thread t(processTask, imagePaths[i], maxImageLength, i);
        t.detach();
    }
}


void processDirectory(const string& directoryPath, int maxImageLength) {
    std::vector<string> imagePaths;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (fs::is_directory(entry)) {
            // Process subdirectory recursively
            processDirectory(entry.path().string(), maxImageLength);
        } else if (fs::is_regular_file(entry)) {
            const string& filePath = entry.path().string();
            const string& fileExtension = entry.path().extension().string();

            if (fileExtension == ".jpg" || fileExtension == ".png" || fileExtension == ".bmp") {
                imagePaths.push_back(filePath);
            }
        }
    }
    runTasks(imagePaths, maxImageLength);
}


string getRelativePath(const string& initialPath, const string& filePath) {
    fs::path initialDir(initialPath);
    fs::path file(filePath);

    fs::path relativePath = file.lexically_relative(initialDir);

    return relativePath.string();
}


std::optional<Mat> readImage(const string& imagePath) {
    Mat image = imread(imagePath);

    if (image.empty()) {
        cerrPlus("Failed to load the image: " + imagePath);
        return std::nullopt;
    }
    return image;
}


void saveImage(const string& outputFileName, const Mat& image, const std::vector<int>& compressionParams) {
    if (compressionParams.empty()) {
        imwrite(outputFileName, image);
        return;
    } else {
        imwrite(outputFileName, image, compressionParams);
        return;
    }
}


std::vector<int> getCompressionParamsForImage(const string& imageType, int imageQuality) {
    std::vector<int> compressionParams;
    if (imageType == "jpg") {
        compressionParams.push_back(IMWRITE_JPEG_QUALITY);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-100)
    } else if (imageType == "png") {
        compressionParams.push_back(cv::IMWRITE_PNG_COMPRESSION);
        imageQuality =  9 - ((imageQuality * 9) / 100);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-9)
    }

    return compressionParams;
}


std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const string& imagePath, const Mat& image, int maxImageLength) {
    int width = image.cols;
    int height = image.rows;

    if (width <= maxImageLength && height <= maxImageLength) {
        return make_tuple(std::nullopt, width, height, -1, -1);
    }

    int newWidth, newHeight;
    if (width > height) {
        newWidth = maxImageLength;
        newHeight = maxImageLength * height / width;
    } else {
        newWidth = maxImageLength * width / height;
        newHeight = maxImageLength;
    }

    Mat resizedImage;
    resize(image, resizedImage, Size(newWidth, newHeight));

    return std::make_tuple(resizedImage, width, height, newWidth, newHeight);
}


void createBackup(const string& dir) {
    // TODO
}


string selectFolder() {
    coutPlus("Select a directory with images...", "blue");

    BROWSEINFO bi;
    char szPath[MAX_PATH] = "";

    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = nullptr;
    bi.pszDisplayName = szPath;
    bi.lpszTitle = "Select a directory with images";
    bi.ulFlags = BIF_RETURNONLYFSDIRS;

    // Open the folder selection dialog
    LPITEMIDLIST pItemIdList = SHBrowseForFolder(&bi);
    if (pItemIdList != nullptr) {
        if (SHGetPathFromIDList(pItemIdList, szPath)) {
            coutPlus("Selected directory: " + string(szPath), "yellow");
        }

        // Free the memory allocated by SHBrowseForFolder
        CoTaskMemFree(pItemIdList);
    } else {
        coutPlus("Directory selection canceled", "red");
        selectFolder();
    }

    return szPath;
}


void coutPlusPlus(const std::vector<std::string>& texts, const std::vector<std::string>& colors, bool endLine) {
    {
        std::lock_guard<std::mutex> lock(mtx);

        for (size_t i = 0; i < texts.size(); ++i) {
            setConsoleTextColor(stoi(getColorEscapeSequence(colors[i])));
            std::cout << texts[i];
            setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
        }
        if (endLine) {
            std::cout << std::endl;
        }
    }
}


void coutPlus(const string& text, const string& color, bool endLine) {
    {
        std::lock_guard<std::mutex> lock(mtx);

        setConsoleTextColor(stoi(getColorEscapeSequence(color)));
        std::cout << text;
        if (endLine) {
            std::cout << std::endl;
        }
        setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
    }
}


void cerrPlus(const string& text) {
    {
        std::lock_guard<std::mutex> lock(mtx);

        setConsoleTextColor(stoi(getColorEscapeSequence("red")));
        std::cerr << text << std::endl;
        setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
    }
}


void setConsoleTextColor(int colorCode) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
}


string getColorEscapeSequence(const string& color) {
    static std::unordered_map<string, int> colorMap = {
            {"DEFAULT", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE},
            {"red", FOREGROUND_RED | FOREGROUND_INTENSITY},
            {"green", FOREGROUND_GREEN | FOREGROUND_INTENSITY},
            {"blue", FOREGROUND_BLUE | FOREGROUND_INTENSITY},
            {"blue2", FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY},
            {"yellow", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY},
            {"yellow2", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY | BACKGROUND_INTENSITY},
            {"pink", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY},
            {"light-blue", FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY}
    };

    auto it = colorMap.find(color);
    if (it != colorMap.end()) {
        int colorCode = it->second;
        return std::to_string(colorCode);
    }

    // Return default color if color is not found
    auto defaultColor = colorMap.find("DEFAULT");
    if (defaultColor != colorMap.end()) {
        int defaultColorCode = defaultColor->second;
        return std::to_string(defaultColorCode);
    }

    return "";
}