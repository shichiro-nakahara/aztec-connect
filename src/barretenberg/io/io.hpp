#pragma once

#include <cstddef>
#include <cstdint>
#include <memory.h>

#include "../assert.hpp"
#include "../curves/bn254/fq.hpp"
#include "../curves/bn254/g1.hpp"
#include "../curves/bn254/g2.hpp"
#include "../types.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

/**
 *
 * i/o methods to read a trusted setup SRS. Original source code from setup-tools/src/aztec_common
 *
 **/

namespace barretenberg {
namespace io {
struct Manifest {
    uint32_t transcript_number;
    uint32_t total_transcripts;
    uint32_t total_g1_points;
    uint32_t total_g2_points;
    uint32_t num_g1_points;
    uint32_t num_g2_points;
    uint32_t start_from;
};

constexpr size_t BLAKE2B_CHECKSUM_LENGTH = 64;

inline bool isLittleEndian()
{
    constexpr int num = 42;
    return (*(char*)&num == 42);
}

inline size_t get_transcript_size(const Manifest& manifest)
{
    const size_t manifest_size = sizeof(Manifest);
    const size_t g1_buffer_size = sizeof(fq) * 2 * manifest.num_g1_points;
    const size_t g2_buffer_size = sizeof(fq2) * 2 * manifest.num_g2_points;
    return manifest_size + g1_buffer_size + g2_buffer_size + BLAKE2B_CHECKSUM_LENGTH;
}

inline void read_manifest(std::vector<char>& buffer, Manifest& manifest)
{
    auto manifest_buf = (Manifest*)&buffer[0];
    std::copy(manifest_buf, manifest_buf + 1, &manifest);
    manifest.transcript_number = ntohl(manifest.transcript_number);
    manifest.total_transcripts = ntohl(manifest.total_transcripts);
    manifest.total_g1_points = ntohl(manifest.total_g1_points);
    manifest.total_g2_points = ntohl(manifest.total_g2_points);
    manifest.num_g1_points = ntohl(manifest.num_g1_points);
    manifest.num_g2_points = ntohl(manifest.num_g2_points);
    manifest.start_from = ntohl(manifest.start_from);
}

inline void read_g1_elements_from_buffer(g1::affine_element* elements, char* buffer, size_t buffer_size)
{
    constexpr size_t bytes_per_element = sizeof(g1::affine_element);
    size_t num_elements = buffer_size / bytes_per_element;

    memcpy((void*)elements, (void*)buffer, buffer_size);
    if (isLittleEndian()) {
        for (size_t i = 0; i < num_elements; ++i) {
            elements[i].x.data[0] = __builtin_bswap64(elements[i].x.data[0]);
            elements[i].x.data[1] = __builtin_bswap64(elements[i].x.data[1]);
            elements[i].x.data[2] = __builtin_bswap64(elements[i].x.data[2]);
            elements[i].x.data[3] = __builtin_bswap64(elements[i].x.data[3]);
            elements[i].y.data[0] = __builtin_bswap64(elements[i].y.data[0]);
            elements[i].y.data[1] = __builtin_bswap64(elements[i].y.data[1]);
            elements[i].y.data[2] = __builtin_bswap64(elements[i].y.data[2]);
            elements[i].y.data[3] = __builtin_bswap64(elements[i].y.data[3]);
            elements[i].x.self_to_montgomery_form();
            elements[i].y.self_to_montgomery_form();
        }
    }
}

inline void read_g2_elements_from_buffer(g2::affine_element* elements, char* buffer, size_t buffer_size)
{
    constexpr size_t bytes_per_element = sizeof(g2::affine_element);
    size_t num_elements = buffer_size / bytes_per_element;

    memcpy((void*)elements, (void*)buffer, buffer_size);

    if (isLittleEndian()) {
        for (size_t i = 0; i < num_elements; ++i) {
            elements[i].x.c0.data[0] = __builtin_bswap64(elements[i].x.c0.data[0]);
            elements[i].x.c0.data[1] = __builtin_bswap64(elements[i].x.c0.data[1]);
            elements[i].x.c0.data[2] = __builtin_bswap64(elements[i].x.c0.data[2]);
            elements[i].x.c0.data[3] = __builtin_bswap64(elements[i].x.c0.data[3]);
            elements[i].y.c0.data[0] = __builtin_bswap64(elements[i].y.c0.data[0]);
            elements[i].y.c0.data[1] = __builtin_bswap64(elements[i].y.c0.data[1]);
            elements[i].y.c0.data[2] = __builtin_bswap64(elements[i].y.c0.data[2]);
            elements[i].y.c0.data[3] = __builtin_bswap64(elements[i].y.c0.data[3]);
            elements[i].x.c1.data[0] = __builtin_bswap64(elements[i].x.c1.data[0]);
            elements[i].x.c1.data[1] = __builtin_bswap64(elements[i].x.c1.data[1]);
            elements[i].x.c1.data[2] = __builtin_bswap64(elements[i].x.c1.data[2]);
            elements[i].x.c1.data[3] = __builtin_bswap64(elements[i].x.c1.data[3]);
            elements[i].y.c1.data[0] = __builtin_bswap64(elements[i].y.c1.data[0]);
            elements[i].y.c1.data[1] = __builtin_bswap64(elements[i].y.c1.data[1]);
            elements[i].y.c1.data[2] = __builtin_bswap64(elements[i].y.c1.data[2]);
            elements[i].y.c1.data[3] = __builtin_bswap64(elements[i].y.c1.data[3]);
            elements[i].x.c0.self_to_montgomery_form();
            elements[i].x.c1.self_to_montgomery_form();
            elements[i].y.c0.self_to_montgomery_form();
            elements[i].y.c1.self_to_montgomery_form();
        }
    }
}

inline size_t get_file_size(std::string const& filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        return 0;
    }
    return (size_t)st.st_size;
}

inline std::vector<char> read_file_into_buffer(std::string const& filename, size_t offset = 0, size_t size = 0)
{
    size_t file_size = size ? size : get_file_size(filename);
    std::vector<char> buffer(file_size);
    std::ifstream file;
    file.open(filename, std::ifstream::binary);
    file.seekg((int)offset);
    file.read(&buffer[0], (int)buffer.size());
    file.close();
    return buffer;
}

inline std::string get_transcript_path(std::string const& dir, size_t num)
{
    return dir + "/transcript" + (num < 10 ? "0" : "") + std::to_string(num) + ".dat";
};

inline bool is_file_exist(std::string const& fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

inline void read_transcript_g1(g1::affine_element* monomials, size_t degree, std::string const& dir)
{
    // read g1 elements at second array position - first point is the basic generator
    monomials[0] = g1::affine_one;

    size_t num = 0;
    size_t num_read = 1;
    std::string path = get_transcript_path(dir, num);

    while (is_file_exist(path) && num_read < degree) {
        Manifest manifest;

        auto buffer = read_file_into_buffer(path);

        read_manifest(buffer, manifest);

        const size_t num_to_read = std::min((size_t)manifest.num_g1_points, degree - num_read);
        const size_t manifest_size = sizeof(Manifest);
        const size_t g1_buffer_size = sizeof(fq) * 2 * num_to_read;

        read_g1_elements_from_buffer(&monomials[num_read], &buffer[manifest_size], g1_buffer_size);

        num_read += num_to_read;
        path = get_transcript_path(dir, ++num);
    }

    if (num == 0) {
        throw std::runtime_error("No input files found.");
    }
}

inline void read_transcript_g2(g2::affine_element& g2_x, std::string const& dir)
{
    Manifest manifest;

    std::string path = get_transcript_path(dir, 0);
    auto buffer = read_file_into_buffer(path);

    read_manifest(buffer, manifest);

    const size_t manifest_size = sizeof(Manifest);

    const size_t g2_buffer_offset = sizeof(fq) * 2 * manifest.num_g1_points;
    const size_t g2_buffer_size = sizeof(fq2) * 2 * 2;

    g2::affine_element* g2_buffer = (g2::affine_element*)(aligned_alloc(32, sizeof(g2::affine_element) * (2)));

    read_g2_elements_from_buffer(g2_buffer, &buffer[manifest_size + g2_buffer_offset], g2_buffer_size);
    g2_x = g2_buffer[0];
    aligned_free(g2_buffer);
}

inline void read_transcript(g1::affine_element* monomials,
                            g2::affine_element& g2_x,
                            size_t degree,
                            std::string const& path)
{
    read_transcript_g1(monomials, degree, path);
    read_transcript_g2(g2_x, path);
}

} // namespace io
} // namespace barretenberg
