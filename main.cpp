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

using namespace std;
using namespace cv;
namespace fs = std::filesystem;


void core();
void coutPlus(const string& text, const string& color="DEFAULT", boolean endLine=true);
void cerrPlus(const string& text);
string getColorEscapeSequence(const string& color);
void setConsoleTextColor(int colorCode);
string selectFolder();
void createBackup(const string& dir);
optional<Mat> resizeImage(const string& imagePath, const Mat& image, int maxImageLength);
void processDirectory(const string& directoryPath, int maxImageLength);
void processTask(const string& imagePath, int maxImageLength, int taskId);
void runTasks(const vector<string>& imagePaths, int maxImageLength);
optional<Mat> readImage(const string& imagePath);
void saveImage(const string& outputFileName, const Mat& image, const vector<int>& compressionParams={});
vector<int> getCompressionParamsForImage(const string& imageType, int imageQuality=100);


mutex mtx;
condition_variable cvThreads;
int activeThreads = 0;


int main() {
    core();
    return 0;
}


void core() {
    const string DIRECTORY = selectFolder();
    int maxImageLength;
    coutPlus("Enter the maximum length of the image's dimensions [px]:\n>> ", "blue", false);
    cin >> maxImageLength;

    createBackup(DIRECTORY);

    unsigned int numThreads = thread::hardware_concurrency();
    coutPlus("Number of threads supported: " + to_string(numThreads), "yellow");

    processDirectory(DIRECTORY, maxImageLength);

    // Wait until all tasks are completed
    unique_lock<mutex> lock(mtx);
    cvThreads.wait(lock, [] { return activeThreads == 0; });


    // TODO print statistics (how many images edited, how much space saved, how much space saved per image on average,
    //  how much time it took for all images, how much time it took per image, etc...

    system("pause");
}


void processTask(const string& imagePath, int maxImageLength, int taskId) {
//    {
//        unique_lock<mutex> lock(mtx);
//        cout << "Processing Task " << taskId << " on Thread " << this_thread::get_id() << endl;
//    }

//    this_thread::sleep_for(chrono::seconds(1));  // useless delay?

    optional<Mat> read_result = readImage(imagePath);
    if (!read_result.has_value()) {
        return;
    }
    Mat img = read_result.value();

    optional<Mat> resized_result = resizeImage(imagePath, img, maxImageLength);
    if (resized_result.has_value()) {
        img = resized_result.value();
    }

    // TODO user should be able to choose at start whether to convert all images to .jpg or keep them as same types
    vector<int> compressionParams = getCompressionParamsForImage("jpg", 50);

    saveImage(imagePath, img, compressionParams);

    {
        lock_guard<mutex> lock(mtx);
        activeThreads--;
    }

    cvThreads.notify_all();
}


void runTasks(const vector<string>& imagePaths, int maxImageLength) {
    unsigned int numTasks = imagePaths.size();
    for (int i = 0; i < numTasks; ++i) {
        // Wait until a thread is available
        unique_lock<mutex> lock(mtx);
        cvThreads.wait(lock, [] { return activeThreads < thread::hardware_concurrency(); });

        activeThreads++;

        thread t(processTask, imagePaths[i], maxImageLength, i);
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


optional<Mat> readImage(const string& imagePath) {
    Mat image = imread(imagePath);

    if (image.empty()) {
        cerrPlus("Failed to load the image: " + imagePath);
        return nullopt;
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
        compressionParams.push_back(IMWRITE_JPEG_QUALITY);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-100)
    } else if (imageType == "png") {
        compressionParams.push_back(cv::IMWRITE_PNG_COMPRESSION);
        imageQuality =  9 - ((imageQuality * 9) / 100);
        compressionParams.push_back(imageQuality); // Set the desired compression level (0-9)
    }

    return compressionParams;
}


optional<Mat> resizeImage(const string& imagePath, const Mat& image, int maxImageLength) {
    int width = image.cols;
    int height = image.rows;

    if (width <= maxImageLength && height <= maxImageLength) {
        return nullopt;
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

    coutPlus("Image " + imagePath + " resized to " + to_string(newWidth) + "x" + to_string(newHeight),"green");

    return resizedImage;
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


void coutPlus(const string& text, const string& color, boolean endLine) {
    {
        lock_guard<mutex> lock(mtx);
        setConsoleTextColor(stoi(getColorEscapeSequence(color)));
        cout << text;
        if (endLine) {
            cout << endl;
        }
        setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
    }
}


void cerrPlus(const string& text) {
    {
        lock_guard<mutex> lock(mtx);
        setConsoleTextColor(stoi(getColorEscapeSequence("red")));
        cerr << text << endl;
        setConsoleTextColor(stoi(getColorEscapeSequence("DEFAULT")));
    }
}


void setConsoleTextColor(int colorCode) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
}


string getColorEscapeSequence(const string& color) {
    static unordered_map<string, int> colorMap = {
            {"DEFAULT", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE},
            {"red", FOREGROUND_RED | FOREGROUND_INTENSITY},
            {"green", FOREGROUND_GREEN | FOREGROUND_INTENSITY},
            {"blue", FOREGROUND_BLUE | FOREGROUND_INTENSITY},
            {"yellow", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY},
            {"pink", FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY}
    };

    auto it = colorMap.find(color);
    if (it != colorMap.end()) {
        int colorCode = it->second;
        return to_string(colorCode);
    }

    // Return default color if color is not found
    auto defaultColor = colorMap.find("DEFAULT");
    if (defaultColor != colorMap.end()) {
        int defaultColorCode = defaultColor->second;
        return to_string(defaultColorCode);
    }

    return "";
}