#include <iostream>
#include "TriangleApplication.h"

int main() {
	try {
		jhb::HelloTriangleApplication app(800, 600);
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}