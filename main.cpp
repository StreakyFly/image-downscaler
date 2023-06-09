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
void processDirectory(const string& directoryPath);
void processTask(const string& imagePath, int taskId);
void runTasks(const vector<string>& imagePaths);
std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const Mat& image);
std::optional<Mat> readImage(const string& imagePath);
void saveImage(const string& outputFileName, const Mat& image, const vector<int>& compressionParams={});
vector<int> getCompressionParamsForImage(const string& imageType);
int getFileSize(const string& filePath);
double getFileSizeInKB(const string& filePath);
string getRelativePath(const string& initialPath, const string& filePath);
string getFileExtension(const string& filePath);
void setMaxImageLength();
void setImageCompressionLevel();
void setConvertImageToJPEG();
std::optional<bool> answerYesNo(const string& answer);
string toLowerCase(const string& str);
bool isStringInList(const string& element, const vector<string>& list);
void printDownscaledImageStats(const string& imagePath, const double& oldImageSize, int oldWidth, int oldHeight, int newWidth, int newHeight);


std::mutex mtx;
std::condition_variable cvThreads;
int activeThreads = 0;
int maxImageLength = 1920;
int imageCompression = 3;  // [0(no compression, highest image quality), 10(max compression, lowest image quality)]
bool convertToJPEG = false; // should all image types be converted to JPEG?


int main() {
    core();
    return 0;
}


void core() {
    const string DIRECTORY = selectFolder();
    setMaxImageLength();
    setImageCompressionLevel();
    setConvertImageToJPEG();

    createBackup(DIRECTORY);

    unsigned int numThreads = std::thread::hardware_concurrency();
    coutPlus("Number of threads supported: " + std::to_string(numThreads), "yellow");

    processDirectory(DIRECTORY);

    // Wait until all tasks are completed
    std::unique_lock<std::mutex> lock(mtx);
    cvThreads.wait(lock, [] { return activeThreads == 0; });


    // TODO print statistics (how many images edited, how much space saved, how much space saved per image on average,
    //  how much time it took for all images, how much time it took per image, etc...

    system("pause");
}


void setMaxImageLength() {
    coutPlus("Enter the maximum length of the image's dimensions [px]:\n>> ", "blue", false);
    std::cin >> maxImageLength;
    string strMaxImageLength = std::to_string(maxImageLength);

    if (maxImageLength >= 4000) {
        coutPlus("Are you sure you meant " + strMaxImageLength + "px?\n>> ", "red", false);
        string answer;
        std::cin >> answer;
        if (answerYesNo(answer) == true) {
            coutPlus("Understood. Maximum length of the image set to " + strMaxImageLength + "px", "yellow");
        } else if (answerYesNo(answer) == false) {
            setMaxImageLength();
        } else {
            coutPlus("Invalid answer.", "red");
            setMaxImageLength();
        }
        return;
    } else if (maxImageLength <= 50) {
        coutPlus("Are you sure you meant just " + strMaxImageLength + "px?\n>> ", "red", false);
        string answer;
        std::cin >> answer;
        if (answerYesNo(answer) == true) {
            coutPlus("Why though...? What did these poor images do to you, to deserve such terrible fate... :(\n>> ", "blue", false);
            string temp;
            std::cin >> temp;
            coutPlus("Oh... then I think IT'S TIME FOR SOME DESTRUCTION! Maximum length of the image set to puny " + strMaxImageLength + "px! \U0001F608", "red");
        } else if (answerYesNo(answer) == false) {
            setMaxImageLength();
        } else {
            coutPlus("Invalid answer.", "red");
            setMaxImageLength();
        }
        return;
    } else if (maxImageLength <= 360) {
        coutPlus("Are you sure you meant just " + strMaxImageLength + "px?\n>> ", "red", false);
        string answer;
        std::cin >> answer;
        if (answerYesNo(answer) == true) {
            coutPlus("Understood. Maximum length of the image set to " + strMaxImageLength + "px", "yellow");
        } else if (answerYesNo(answer) == false) {
            setMaxImageLength();
        } else {
            coutPlus("Invalid answer.", "red");
            setMaxImageLength();
        }
        return;
    }
}


void setImageCompressionLevel() {
    coutPlus("Enter the image compression level [0-10 (no compression-max compression)]:\n>> ", "blue", false);
    std::cin >> imageCompression;
    string text;

    if (imageCompression > 7) {
        // TODO ask user if they're sure, as this results in terrible imge quality
    }

    // TODO improve these sentences after testing image quality
    static std::unordered_map<int, string> textMap = {
            {0, "Image compression set to NONE. This will result in:\n - largest file size, but\n - highest image quality."},
            {1, "Image compression set to low. This will result in:\n - big file size, but\n - good image quality."},
            {2, "Image compression set to low. This will result in:\n - big file size, but\n - good image quality."},
            {3, "Image compression set to quite low. This will result in:\n - quite big file size, but\n - pretty good image quality."},
            {4, "Image compression set to medium. This will result in:\n - medium file size, and\n - medium image quality."},
            {5, "Image compression set to medium. This will result in:\n - medium file size, and\n - medium image quality."},
            {6, "Image compression set to medium. This will result in:\n - medium file size, and\n - medium image quality."},
            {7, "Image compression set to high. This will result in:\n - small file size, but\n - low image quality."},
            {8, "Image compression set to near maximum. This will result in:\n - small file size, but\n - low image quality."},
            {9, "Image compression set to near maximum. This will result in:\n - small file size, but\n - terrible image quality."},
            {10, "Image compression set to MAXIMUM. This will result in:\n - smallest file size, but\n - terrible image quality."}
    };

    auto it = textMap.find(imageCompression);
    if (it != textMap.end()) {
        text = it->second;
        coutPlus(text, "yellow");
    } else {
        coutPlus("Invalid image compression level. Must be between 0-10.", "red");
        setImageCompressionLevel();
    }
}


void setConvertImageToJPEG() {
    coutPlus("Should all image types be converted to JPEG? [Y/N]:\n>> ", "blue", false);
    string answer;
    std::cin >> answer;
    if (answerYesNo(answer) == true) {
        convertToJPEG = true;
        coutPlus("All images will be converted to JPEG.", "yellow");
    } else if (answerYesNo(answer) == false) {
        convertToJPEG = false;
        coutPlus("Images will NOT be converted to JPEG.", "yellow");
    } else {
        coutPlus("Invalid answer.", "red");
        setConvertImageToJPEG();
    }
}


std::optional<bool> answerYesNo(const string& answer) {
    vector<string> yesAnswers = {"y", "ye", "yes", "d", "da"};
    vector<string> noAnswers = {"n", "no", "ne"};
    string lowercaseAnswer = toLowerCase(answer);

    if (isStringInList(lowercaseAnswer, yesAnswers)) {
        return true;
    } else if (isStringInList(lowercaseAnswer, noAnswers)) {
        return false;
    } else {
        return std::nullopt;
    }
}


string toLowerCase(const string& str) {
    string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}


bool isStringInList(const string& element, const vector<string>& list) {
    return std::find(list.begin(), list.end(), element) != list.end();
}


void processTask(const string& imagePath, int taskId) {
    double oldImageSize = getFileSizeInKB(imagePath);
//    {
//        std::unique_lock<std::mutex> lock(mtx);
//        std::cout << "Processing Task " << taskId << " on Thread " << std::this_thread::get_id() << std::endl;
//    }

    std::optional<Mat> readResult = readImage(imagePath);
    if (!readResult.has_value()) {
        return;
    }
    Mat img = readResult.value();

    auto resizeResult = resizeImage(img);
    if (get<0>(resizeResult).has_value()) {
        img = get<0>(resizeResult).value();
    }

    string fileExtension;
    if (convertToJPEG == true) {
        fileExtension = ".jpg";
    } else {
        fileExtension = getFileExtension(imagePath);
    }
    vector<int> compressionParams;
    compressionParams = getCompressionParamsForImage(fileExtension);

    // TODO compress image and compare size to resized image, if compressed size is smaller, save resized&compressed image, otherwise save just resized image.
    saveImage(imagePath, img, compressionParams);

    {
        std::lock_guard<std::mutex> lock(mtx);
        activeThreads--;
    }

    cvThreads.notify_all();

    printDownscaledImageStats(imagePath, oldImageSize,get<1>(resizeResult), get<2>(resizeResult), get<3>(resizeResult), get<4>(resizeResult));
}


void printDownscaledImageStats(const string& imagePath, const double& oldImageSize, int oldWidth, int oldHeight, int newWidth, int newHeight) {
    double newImageSize = getFileSizeInKB(imagePath);
    std::ostringstream oss1;
    oss1 << std::fixed << std::setprecision(2) << newImageSize;
    string strNewImageSize = oss1.str();
    std::ostringstream oss2;
    oss2 << std::fixed << std::setprecision(2) << oldImageSize;
    string strOldImageSize = oss2.str();

    string msg = std::format("{}", imagePath);
    vector<string> texts;
    vector<string> colors;
    vector<int> paddings = {26, 34, 0};
    vector<string> alignments = {"left", "left", "left"};
    string fileSizeDiff;
    string fileSizeDiffColor;

    if (oldImageSize > newImageSize) {
        fileSizeDiff = std::format("{}kB > {}kB", strOldImageSize, strNewImageSize);
        fileSizeDiffColor = "green";
    } else {
        fileSizeDiff = std::format("{}kB < {}kB", strOldImageSize, strNewImageSize);
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
}


string getFileExtension(const string& filePath) {
    fs::path path(filePath);
    return path.extension().string();
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


void runTasks(const vector<string>& imagePaths) {
    unsigned int numTasks = imagePaths.size();
    for (int i = 0; i < numTasks; ++i) {
        // Wait until a thread is available
        std::unique_lock<std::mutex> lock(mtx);
        cvThreads.wait(lock, [] { return activeThreads < std::thread::hardware_concurrency(); });

        activeThreads++;

        std::thread t(processTask, imagePaths[i], i);
        t.detach();
    }
}


void processDirectory(const string& directoryPath) {
    vector<string> imagePaths;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (fs::is_directory(entry)) {
            // Process subdirectory recursively
            processDirectory(entry.path().string());
        } else if (fs::is_regular_file(entry)) {
            const string& filePath = entry.path().string();
            const string& fileExtension = entry.path().extension().string();

            std::vector<string> supportedExtensions = {".jpg", ".jpeg", ".png", "webp"};
            if (isStringInList(toLowerCase(fileExtension), supportedExtensions)) {
                imagePaths.push_back(filePath);
            }
        }
    }
    runTasks(imagePaths);
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
    if (convertToJPEG == true && compressionParams.empty() == false) {
        // TODO outputFileName = outputFileName(without_previous_extension).jpg
        // TODO delete original image (separate function)
    }
    if (compressionParams.empty()) {
        imwrite(outputFileName, image);
        return;
    } else {
        imwrite(outputFileName, image, compressionParams);
        return;
    }
}


vector<int> getCompressionParamsForImage(const string& imageType) {
    vector<int> compressionParams;
    if (imageType == ".jpg" || imageType == ".jpeg") {
        compressionParams.push_back(cv::IMWRITE_JPEG_QUALITY);
        int imgQuality = 100 - (imageCompression * 10);
        // Set the desired compression level [0(max compression/lowest image quality), 100(no compression/highest image quality)]
        compressionParams.push_back(imgQuality);
    } else if (imageType == ".png") {
        compressionParams.push_back(cv::IMWRITE_PNG_COMPRESSION);
        int imgCompression =  static_cast<int>(imageCompression * 9 / 10.0);
        // Set the desired compression level [0(no compression/highest image quality), 9(max compression/lowest image quality)]
        compressionParams.push_back(imgCompression);
    } else if (imageType == ".webp") {
        compressionParams.push_back(cv::IMWRITE_WEBP_QUALITY);
        int imgQuality = 100 - (imageCompression * 10);
        // Set the desired compression level [0(max compression/lowest image quality), 100(no compression/highest image quality)]
        compressionParams.push_back(imgQuality);
    }
//    } else if (imageType == ".tiff") {
//        compressionParams.push_back(cv::IMWRITE_TIFF_COMPRESSION);
//        int imgCompression = -1;  // TODO - different types of compression for .tiff
////      Set the desired compression level [0(no compression/best image quality), 9(max compression/worst image quality)]
//        compressionParams.push_back(imgCompression);
//    }

    return compressionParams;
}


std::tuple<std::optional<Mat>, int, int, int, int> resizeImage(const Mat& image) {
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