#include "pch.h"
#include "Texture2D.h"
#include "Charon/Core/Application.h"
#include <stb/stb_image.h>

namespace Charon {

	Texture2D::Texture2D(const std::string& path)
		: m_Path(path)
	{
		// Load image from disk
		int width, height, bpp;
		stbi_set_flip_vertically_on_load(true);
		uint8_t* data = stbi_load(path.c_str(), &width, &height, &bpp, 4);
		CR_ASSERT(data, "Failed to load image");

		// Set width and height
		m_Width = width;
		m_Height = height;
		 
		// Create image
		ImageSpecification imageSpecification = {};
		imageSpecification.Data = data;
		imageSpecification.Width = m_Width;
		imageSpecification.Height = m_Width;
		imageSpecification.Format = VK_FORMAT_R8G8B8A8_UNORM;
		imageSpecification.UseStagingBuffer = true;

		m_Image = CreateRef<Image>(imageSpecification);

		// Free CPU memory
		stbi_image_free(data);
	}

	Texture2D::~Texture2D()
	{
	}

}