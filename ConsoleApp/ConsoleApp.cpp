#include "stdafx.h"

#include "ImageOptimizer.h"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>

namespace po = boost::program_options;
namespace fs = boost::filesystem;



int main(int argc, char* argv[])
{
	std::string input;
	po::options_description desc("Allowed options");

	desc.add_options()
		("help,h", "produce help message")
		("input,i", po::value<std::string>(&input)->default_value("."), "image or folder to process");

	po::positional_options_description p;
	p.add("input", -1);

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	po::notify(vm);

	if (!fs::exists(input))
	{
		std::cout << "Input file or folder doesn't exist" << std::endl;

		return 1;
	}

	std::cout << ImageOptimizer::GetVersion() << std::endl;

	ImageOptimizer::EnableFileLogging();

	ImageOptimizer imageOptimizer;

	try
	{
		if (fs::is_regular_file(input))
		{
			imageOptimizer.OptimizeImage(input, 0.9999f);
		}
		else if (fs::is_directory(input))
		{
			imageOptimizer.OptimizeFolderRecursive(input, 0.9999f);
		}
		else
		{
			std::cout << "Input isn't a regular file or directory" << std::endl;

			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "Error during optimization:" << std::endl;
		std::cout << e.what();
	}

	std::cout << "Done!";

	std::getchar();

    return 0;
}

