#include "libWEBP.hpp"

#include <absl/log/log.h>
#include <webp/decode.h>
#include <webp/encode.h>

#include <cstdint>
#include <memory>

bool WebPImage::read(const std::filesystem::path& filename) {
    int width = 0;
    int height = 0;
    uint8_t* decoded_data = nullptr;
    FILE* file = nullptr;
    
    LOG(INFO) << "Loading image: " << filename;

    // Open the file for reading in binary mode
    file = fopen(filename.string().c_str(), "rb");
    if (!file) {
        LOG(ERROR) << "Can't open file " << filename;
        return false;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    auto data = std::make_unique<uint8_t[]>(file_size);
    fread(data.get(), 1, file_size, file);
    fclose(file);

    decoded_data = WebPDecodeRGBA(data.get(), file_size, &width, &height);

    if (decoded_data == nullptr) {
        LOG(ERROR) << "Couldn't decode image data";
        return false;
    }

    width_ = width;
    height_ = height;
    data_ = std::unique_ptr<uint8_t[]>(decoded_data);

    return true;
}

void WebPImage::rotate_image_90() { rotateImage(Degrees::DEGREES_90); }

void WebPImage::rotate_image_180() { rotateImage(Degrees::DEGREES_180); }

void WebPImage::rotate_image_270() { rotateImage(Degrees::DEGREES_270); }

void WebPImage::to_greyscale() {
    if (data_ == nullptr) {
        LOG(ERROR) << "No image data to convert to greyscale";
        return;
    }
    LOG(INFO) << "Converting image to greyscale";

    for (int i = 0; i < width_ * height_; ++i) {
        // Convert each pixel to greyscale by averaging RGB values
        uint8_t grey = (data_[i * 4] + data_[i * 4 + 1] + data_[i * 4 + 2]) / 3;
        data_[i * 4] = grey;
        data_[i * 4 + 1] = grey;
        data_[i * 4 + 2] = grey;
    }
}

bool WebPImage::write(const std::filesystem::path& filename) {
    if (data_ == nullptr) {
        LOG(ERROR) << "No image data to write";
        return false;
    }

    FILE* file = fopen(filename.string().c_str(), "wb");
    if (file == nullptr) {
        LOG(ERROR) << "Can't open file " << filename;
        return false;
    }

    int stride = width_ * 4;
    uint8_t* output = nullptr;
    size_t output_size = 0;

    output_size = WebPEncodeRGBA(data_.get(), width_, height_, stride, .5F, &output);
    if (output_size == 0) {
        LOG(ERROR) << "Failed to encode WebP image";
        fclose(file);
        return false;
    }

    fwrite(output, 1, output_size, file);
    fclose(file);

    LOG(INFO) << "New WebP image written to " << filename;

    return true;
}

void WebPImage::rotateImage(Degrees degrees) {
    if (data_ == nullptr) {
        LOG(ERROR) << "No image data to rotate";
        return;
    }
    DLOG(INFO) << "Rotating image";

    long rotated_width = 0;
    long rotated_height = 0;
    std::unique_ptr<uint8_t[]> rotated_data = nullptr;

    switch (degrees) {
        case Degrees::DEGREES_90:
            rotated_width = height_;
            rotated_height = width_;
            rotated_data =
                std::make_unique<uint8_t[]>(rotated_width * rotated_height * 4);
            for (int y = 0; y < height_; ++y) {
                for (int x = 0; x < width_; ++x) {
                    int source_index = (y * width_ + x) * 4;
                    int dest_index = ((width_ - 1 - x) * rotated_width + y) * 4;
                    for (int k = 0; k < 4; ++k) {
                        rotated_data[dest_index + k] = data_[source_index + k];
                    }
                }
            }
            break;
        case Degrees::DEGREES_180:
            rotated_width = width_;
            rotated_height = height_;
            rotated_data =
                std::make_unique<uint8_t[]>(rotated_width * rotated_height * 4);
            for (int y = 0; y < height_; ++y) {
                for (int x = 0; x < width_; ++x) {
                    int source_index = (y * width_ + x) * 4;
                    int dest_index =
                        ((height_ - 1 - y) * width_ + (width_ - 1 - x)) * 4;
                    for (int k = 0; k < 4; ++k) {
                        rotated_data[dest_index + k] = data_[source_index + k];
                    }
                }
            }
            break;
        case Degrees::DEGREES_270:
            rotated_width = height_;
            rotated_height = width_;
            rotated_data =
                std::make_unique<uint8_t[]>(rotated_width * rotated_height * 4);
            for (int y = 0; y < height_; ++y) {
                for (int x = 0; x < width_; ++x) {
                    int source_index = (y * width_ + x) * 4;
                    int dest_index =
                        (x * rotated_width + (height_ - 1 - y)) * 4;
                    for (int k = 0; k < 4; ++k) {
                        rotated_data[dest_index + k] = data_[source_index + k];
                    }
                }
            }
            break;
        default:
            return;
    }

    data_ = std::move(rotated_data);
    width_ = rotated_width;
    height_ = rotated_height;
}
