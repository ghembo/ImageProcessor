#include "ImageOptimizer.h"

#include "Logger.h"
#include "JpegEncoderDecoder.h"
#include "OptimizationResult.h"
#include "ImageProcessor.h"

#include "opencv2\core\core.hpp"

#include <string>
#include <regex>
#include <future>
#include <filesystem>
#include <numeric>

namespace fs = std::experimental::filesystem;

#define VERSION "0.0.0.0_OPENCV" CV_VERSION

const std::string ImageOptimizer::s_version = VERSION;

std::string ImageOptimizer::GetVersion()
{
	return s_version;
}

ImageOptimizer::ImageOptimizer() :
	m_imageProcessor(new ImageProcessor(m_logger))
{
}

ImageOptimizer::~ImageOptimizer() = default;

void ImageOptimizer::SetLogCallbacks(traceCallback_t traceCallback, warningCallback_t warningCallback, errorCallback_t errorCallback)
{
	m_logger.setCallbacks(traceCallback, warningCallback, errorCallback);
}

OptimizationResult ImageOptimizer::OptimizeFolder(const std::string& imageFolderPath, ImageSimilarity::Similarity similarity)
{
	m_logger.trace(imageFolderPath);

	validateFolderPath(imageFolderPath);

	auto filenames = getJpegInFolder(imageFolderPath);

	return parallelOptimizeImages(filenames, similarity);
}

OptimizationResult ImageOptimizer::OptimizeFolderRecursive(const std::string& imageFolderPath, ImageSimilarity::Similarity similarity)
{
	validateFolderPath(imageFolderPath);

	std::vector<std::string> filenames{ getJpegInFolder(imageFolderPath) };

	auto folders = getAllFoldersInFolder(imageFolderPath);

	for (const auto& folder : folders)
	{
		auto images{ getJpegInFolder(folder) };
		filenames.insert(filenames.end(), images.begin(), images.end()); // v1.insert(v1.end(), make_move_iterator(v2.begin()), make_move_iterator(v2.end()));
	}

	return parallelOptimizeImages(filenames, similarity);
}

OptimizationResult ImageOptimizer::optimizeImages(const iterator_t& first, const iterator_t& last, ImageSimilarity::Similarity similarity)
{
	OptimizationResult result;

	for (auto image = first; image != last; ++image)
	{
		try
		{
			result += OptimizeImage(*image, similarity);
		}
		catch (const std::exception& e)
		{
			m_logger.trace("Error during optimization, unable to process image " + *image + ": \n" + std::string(e.what()));
		}
	}

	return result;
}

OptimizationResult ImageOptimizer::parallelOptimizeImages(const std::vector<std::string>& filenames, ImageSimilarity::Similarity similarity)
{
	const auto nthreads = std::thread::hardware_concurrency();
	const auto nfiles = filenames.size();
	const int imagesPerThread = nfiles / nthreads;

	std::vector<std::future<OptimizationResult>> futures;

	for (size_t t = 0; t < nthreads; t++)
	{
		size_t firstImage = imagesPerThread * t;
		size_t lastImage = imagesPerThread * (t + 1);

		futures.push_back(std::async(std::launch::async, [=]() {return optimizeImages(filenames.cbegin() + firstImage, filenames.cbegin() + lastImage, similarity); }));
	}

	size_t first = imagesPerThread * nthreads;

	OptimizationResult result = optimizeImages(filenames.cbegin() + first, filenames.cend(), similarity);

	return std::accumulate(futures.begin(), futures.end(), result, [](auto& total, auto& future) {return total + future.get(); });
}

OptimizationResult ImageOptimizer::OptimizeImage( const std::string& imagePath, ImageSimilarity::Similarity similarity)
{
 	m_logger.trace(imagePath.data());

	auto image = loadImage(imagePath);

	validateImage(image);

	m_logger.trace("Target ssim: " + std::to_string(similarity.GetValue()));
	
	auto bestQuality = m_imageProcessor->OptimizeImage(image, similarity);

	image.release();

	image = JpegEncoderDecoder::LoadColorImage(imagePath);
		
	auto temporaryFilename(getSuffixedFilename(imagePath, "_tmp"));

	JpegEncoderDecoder::SaveJpeg(image, temporaryFilename, bestQuality);

	OptimizationResult result{ fs::file_size(imagePath) , fs::file_size(temporaryFilename) };

	logFileSizesAndCompression(result);

	auto newFileName(getSuffixedFilename(imagePath, "_compressed"));

	if (!result.IsCompressed())
	{
		fs::remove(temporaryFilename);

		m_logger.trace("Couldn't compress more");

		fs::copy_file(imagePath, newFileName);

		result = result.GetUncompressedResult();
	}
	else
	{
		rename(temporaryFilename.c_str(), newFileName.c_str());
	}

	return result;
}

cv::Mat ImageOptimizer::loadImage(const std::string& imagePath)
{
	validateImagePath(imagePath);

	return JpegEncoderDecoder::LoadGrayscaleImage(imagePath);
}

void ImageOptimizer::validateFolderPath(const std::string& imageFolderPath)
{
	fs::path p(imageFolderPath);

	if (!fs::exists(p))
	{
		handleInvalidArgument("Folder doesn't exist");
	}

	if (!fs::is_directory(p))
	{
		handleInvalidArgument("Path is not a folder");
	}
}

void ImageOptimizer::validateImagePath(const std::string& imagePath)
{
	fs::path p(imagePath);

	if (!fs::exists(p))
	{
		handleInvalidArgument("File doesn't exist");
	}

	if (!fs::is_regular_file(p))
	{
		handleInvalidArgument("Path is not a file");
	}
}

void ImageOptimizer::validateImage(const cv::Mat& image)
{
	if (image.data == NULL)
	{
		handleInvalidArgument("Image format not supported");
	}

	if (!image.isContinuous())
	{
		handleInvalidArgument("Cannot efficiently process input image");
	}
}

void ImageOptimizer::handleInvalidArgument(const char* message)
{
	m_logger.trace(message);
	throw std::invalid_argument(message);
}

std::string ImageOptimizer::addSuffixToFileName(const std::string& filename, const std::string& suffix)
{
	fs::path p(filename);

	fs::path newFilename = p.parent_path() / fs::path(p.stem().string() + suffix + p.extension().string());

	return newFilename.string();
}

std::string ImageOptimizer::getSuffixedFilename(const std::string& filename, const std::string& suffix)
{
	for (unsigned long long counter = 0; ; counter++)
	{
		auto temporaryFileName = addSuffixToFileName(filename, suffix + std::to_string(counter));

		if (!fs::exists(temporaryFileName))
		{
			return temporaryFileName;
		}
	}
}

bool ImageOptimizer::isJpegFile(const fs::directory_entry& file)
{
	if (!fs::is_regular_file(file))
	{
		return false;
	}

	const std::regex jpegExtension(R"(^\.jpe?g$)", std::regex_constants::icase);

	return std::regex_match(file.path().extension().string(), jpegExtension);
}

void ImageOptimizer::logFileSizesAndCompression(OptimizationResult optimizationResult)
{
	auto compression = optimizationResult.GetCompressionPercentage();

	m_logger.trace("Original size: " + std::to_string(optimizationResult.GetOriginalSize()) +
				" New size: " + std::to_string(optimizationResult.GetCompressedSize()) + 
				" Compression: " + std::to_string(compression) + "%");
}

std::vector<std::string> ImageOptimizer::getJpegInFolder(const std::string& imageFolderPath)
{
	std::vector<std::string> filenames;

	for (auto& file : fs::directory_iterator(fs::path(imageFolderPath)))
	{
		if (isJpegFile(file))
		{
			filenames.push_back(file.path().string());
		}
	}

	return filenames;
}

std::vector<std::string> ImageOptimizer::getAllFoldersInFolder(const std::string& folderPath)
{
	std::vector<std::string> folders;

	for (auto& file : fs::recursive_directory_iterator(fs::path(folderPath)))
	{
		if (is_directory(file))
		{
			folders.push_back(file.path().string());
		}
	}

	return folders;
}