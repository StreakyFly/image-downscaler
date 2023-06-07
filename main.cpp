#include <iostream>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <filesystem>
#include <thread>
#include <mutex>
#include <format>
#include <fstream>
#include <windows.h>
#include <shlobj.h>
#include <opencv2/opencv.hpp>


namespace fs = std::filesystem;
using std::string;
using std::vector;
using cv::Mat;


void core();
void coutPlus(const string& text, const string& color="DEFAULT", bool endLine=true);
void cerrPlus(const string& text);
void coutPlusPlus(const vector<string>& texts, const vector<string>& colors, bool endLine=true, const vector<int>& paddings={}, const vector<string>& alignments={});
string getColorEscapeSequence(const string& color);
void setConsoleTextColor(int colorCode);
string selectFolder();
void createBackup(const string& dir);
void processDirectory(const string& directoryPath, int maxImageLength);
void processTask(const string& imagePath, int maxImageLength, int taskId);
void runTasks(const vector<string>& imagePaths, int maxImageLength);
std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const Mat& image, int maxImageLength);
std::optional<Mat> readImage(const string& imagePath);
void saveImage(const string& outputFileName, const Mat& image, const vector<int>& compressionParams={});
vector<int> getCompressionParamsForImage(const string& imageType, int imageQuality=100);
int getFileSize(const string& filePath);
double getFileSizeInKB(const string& filePath);
string getRelativePath(const string& initialPath, const string& filePath);


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

    auto resizeResult = resizeImage(img, maxImageLength);
    if (get<0>(resizeResult).has_value()) {
        img = get<0>(resizeResult).value();
    }

    // TODO user should be able to choose at start whether to convert all images to .jpg or keep them as same types
    vector<int> compressionParams = getCompressionParamsForImage("jpg", 70);

    saveImage(imagePath, img, compressionParams);

    // TODO put this in a separate function printDownscaledStats() or smt like dat
    int oldWidth = get<1>(resizeResult);
    int oldHeight = get<2>(resizeResult);
    int newWidth = get<3>(resizeResult);
    int newHeight = get<4>(resizeResult);

    double newImageSize = getFileSizeInKB(imagePath);
    newImageSize = std::round(newImageSize * 100) / 100;  // Round to two decimal places

    string msg = std::format("{}", imagePath);
    vector<string> texts;
    vector<string> colors;
    vector<int> paddings = {26, 34, 0};
    vector<string> alignments = {"left", "left", "left"};
    string fileSizeDiff;
    string fileSizeDiffColor;

    if (oldImageSize > newImageSize) {
        fileSizeDiff = std::format("{}kB > {}kB", oldImageSize, newImageSize);
        fileSizeDiffColor = "green";
    } else {
        fileSizeDiff = std::format("{}kB < {}kB", oldImageSize, newImageSize);
        fileSizeDiffColor = "red";
    }

    string oldDimensions = std::format("[{}x{}]", oldWidth, oldHeight);
    string newDimensions = std::format("[{}x{}]", newWidth, newHeight);

    if (newWidth == -1) {
        string notDownscaled = std::format("{} (not downscaled)", oldDimensions);
        texts = {fileSizeDiff, notDownscaled, msg};
        colors = {fileSizeDiffColor, "yellow", "light-yellow"};
    } else {
        string downscaled = std::format("{} => {}", oldDimensions, newDimensions);
        texts = {fileSizeDiff, downscaled, msg};
        colors = {fileSizeDiffColor, "blue", "light-yellow"};
    }
    coutPlusPlus(texts, colors, true, paddings, alignments);

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
    return (int) fileSize;
}


double getFileSizeInKB(const string& filePath) {
    std::streampos fileSize = getFileSize(filePath);
    if (fileSize == -1) {
        cerrPlus("Failed to get image file size.");
    }
    double sizeKB = (double) fileSize / 1024.0;

    return sizeKB;
}


void runTasks(const vector<string>& imagePaths, int maxImageLength) {
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
    vector<string> imagePaths;
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
    Mat image = cv::imread(imagePath);

    if (image.empty()) {
        cerrPlus("Failed to load the image: " + imagePath);
        return std::nullopt;
    }
    return image;
}


void saveImage(const string& outputFileName, const Mat& image, const vector<int>& compressionParams) {
    if (compressionParams.empty()) {
        imwrite(outputFileName, image);
        return;
    } else {
        imwrite(outputFileName, image, compressionParams);
        return;
    }
}


vector<int> getCompressionParamsForImage(const string& imageType, int imageQuality) {
    vector<int> compressionParams;
    if (imageType == "jpg") {
        compressionParams.push_back(cv::IMWRITE_JPEG_QUALITY);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-100)
    } else if (imageType == "png") {
        compressionParams.push_back(cv::IMWRITE_PNG_COMPRESSION);
        imageQuality =  9 - ((imageQuality * 9) / 100);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-9)
    }

    return compressionParams;
}


std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const Mat& image, int maxImageLength) {
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
    resize(image, resizedImage, cv::Size(newWidth, newHeight));

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


void coutPlusPlus(const vector<string>& texts, const vector<string>& colors, bool endLine, const vector<int>& paddings, const vector<string>& alignments) {
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (paddings.empty()) {
            for (size_t i = 0; i < texts.size(); ++i) {
                setConsoleTextColor(stoi(getColorEscapeSequence(colors[i])));
                std::cout << texts[i];
                setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
            }
        }
        else {
            for (size_t i = 0; i < texts.size(); ++i) {
                std::cout << std::setfill('.') << std::setw(paddings[i]);
                if (alignments[i] == "left") std::cout << std::left;
                if (alignments[i] == "right") std::cout << std::right;
                setConsoleTextColor(stoi(getColorEscapeSequence(colors[i])));
                std::cout << texts[i];
                setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
            }
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
        setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
        std::cerr << text << std::endl;
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
            {"yellow", FOREGROUND_RED | FOREGROUND_GREEN},
            {"light-yellow", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY},
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