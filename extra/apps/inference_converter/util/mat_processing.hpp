#ifndef MAT_PROCESSING_H
#define MAT_PROCESSING_H

#include <iostream>
#include <memory>
#include <string>       

#include "exception.hpp"
#include "matio.h"


template <typename Dtype> enum matio_classes matio_class_map();
template <> enum matio_classes matio_class_map<float>() { return MAT_C_SINGLE; }
template <> enum matio_classes matio_class_map<double>() { return MAT_C_DOUBLE; }
template <> enum matio_classes matio_class_map<int>() { return MAT_C_INT32; }
template <> enum matio_classes matio_class_map<unsigned int>() { return MAT_C_UINT32; }

/*
 * Reads .mat file 'fileName' and writes data to 'data'.
 * Transpose stands for transposition of elements from [channels][rows][cols] order
 * to [rows][cols][channels] order.
*/
template <typename T>
void LoadMatFile(const std::string& fileName, T* data, size_t maxDataSize, int rows, int cols,
                 int& channels, bool transpose = false, 
                 void* buffer = NULL, size_t bufferSize = 0
                );

/*
 * Transforms row-ordered matrix to column-ordered.
*/
template <typename T>
void ReshapeToMatlabFormat(const T* map, int rows, int cols, T* result);


template <typename T>
void LoadMatFile(const std::string& fileName, T* data, size_t maxDataSize, int rows, int cols, int& channels, 
                 bool transpose, void* buffer, size_t bufferSize) 
{
    if (data == NULL) {
        throw exception("Data pointer is null.");
    }

    mat_t* matfp = Mat_Open(fileName.c_str(), MAT_ACC_RDONLY);
    if (matfp == NULL) {
        throw exception("Error opening MAT file: '" + fileName + "'.");
    }

    // Read data
    matvar_t* matvar = Mat_VarReadInfo(matfp, "data");
    if (matvar == NULL) {
        Mat_Close(matfp);
        throw exception("Field 'data' not present. In MAT file: '" + fileName + "'.");
    }

    if (matvar->class_type != matio_class_map<T>()) {
        Mat_VarFree(matvar);
        Mat_Close(matfp);
        throw exception("Field 'data' must be of the right class (single/double). In MAT file: '" + fileName + "'.");
    }
    if ((4 <= matvar->rank) && (matvar->dims[3] != 1)) {
        Mat_VarFree(matvar);
        Mat_Close(matfp);
        throw exception("Field 'data' cannot have ndims > 3. In MAT file: '" + fileName + "'.");
    }

    const int matCols = matvar->dims[0];
    const int matRows = matvar->dims[1];
    channels = matvar->dims[2];
    const int fileSize = matCols * matRows * channels;

    int dataSize = rows * cols * channels;
    if (maxDataSize < dataSize) {
        dataSize = maxDataSize;
    }

    if ((matCols < cols) || (matRows < rows)) {
        Mat_VarFree(matvar);
        Mat_Close(matfp);     
        throw exception("Size of requesting matrix is bigger than size of matrix in file.");
    }

    T* fileData = reinterpret_cast<T*>(buffer);
    bool fileDataIsNew = false;
    if ((buffer == NULL) || (bufferSize < fileSize * sizeof(T))) {
        fileData = new T[fileSize];
        fileDataIsNew = true;
    }
    
    const auto res = Mat_VarReadDataLinear(matfp, matvar, fileData, 0, 1, fileSize);
    Mat_VarFree(matvar);
    Mat_Close(matfp);

    if (res != 0) {
        if (fileDataIsNew == true) {
            delete[] fileData;
        }
        throw exception("Error reading array 'data' from MAT file: '" + fileName + "'.");
    }


    const int channelSize = matRows * matCols;

    // extract from fileData and crop mat file to image size (cols, rows)
    if (transpose == true) {
        const int out_offset = cols * channels;

        for (int channel = 0; channel < channels; ++channel) {
            for (int row = 0; row < rows; ++row) {
                for (int column = 0; column < cols; ++column) {
                    // fileData[channel][row][column] -> data[row][column][channel]
                    const int inInd  = column  + row    * matCols  + channel * channelSize;
                    const int outInd = channel + column * channels + row     * out_offset;                                        
                    data[outInd] = fileData[inInd];
                }
            }
        }
    } else {
        const size_t outputChannelSize = rows * cols;
        for (int channel = 0; channel < channels; ++channel) {
            for (int row = 0; row < rows; ++row) {
                for (int column = 0; column < cols; ++column) {
                    // fileData[channel][column?][row]-> data[channel][column][row]
                    const int inInd  = row + column * matCols + channel * channelSize;
                    const int outInd = row + column * rows    + channel * rows * cols; 
                    data[outInd] = fileData[inInd]; 
                }
            }
        }
    }

    if (fileDataIsNew == true) {
        delete[] fileData;
    }
}

template <typename T>
void ReshapeToMatlabFormat(const T* map, int rows, int cols, T* result) {
    for (int h = 0; h < rows; ++h) {
        for (int w = 0; w < cols; ++w) {
            const int out_index = w * rows + h;
            const int in_index  = h * cols + w;
            result[out_index] = map[in_index];
        }
    }
}

#endif // MAT_PROCESSING_H