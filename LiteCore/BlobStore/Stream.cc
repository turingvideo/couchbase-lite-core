//
// Stream.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Stream.hh"
#include "Error.hh"
#include "Logging.hh"
#include "PlatformIO.hh"
#include <errno.h>
#include <memory>

namespace litecore {
    using namespace std;

    
    alloc_slice ReadStream::readAll() {
        uint64_t length = getLength();
        if (length > SIZE_MAX)    // overflow check for 32-bit
            throw bad_alloc();
        auto contents = alloc_slice((size_t)length);
        contents.shorten(read((void*)contents.buf, contents.size));
        return contents;
    }

    
    static void checkErr(fstream *file) {
        if (_usuallyFalse(file->fail()))
            error::_throwErrno();
    }


    FileReadStream::FileReadStream(const fs::path &path, std::ios_base::openmode mode) {
        _file = new fstream();
        _file->open(path, mode);
        if (_file->fail())
            error::_throwErrno();
    }


    void FileReadStream::close() {
        auto file = _file;
        _file = nullptr;
        if (file) {
            file->close();
            if(file->fail()) {
                error::_throwErrno();
            }
        }
    }


    FileReadStream::~FileReadStream() {
        if (_file) {
            // Destructor cannot throw exceptions, so just warn if there was an error:
            _file->close();
            if (_file->fail())
                Warn("FileStream destructor: fclose got error %d", errno);
        }
    }


    uint64_t FileReadStream::getLength() const {
		if(!_file) {
			return 0;
		}

        uint64_t curPos = _file->tellg();
        _file->seekg(0, _file->end);
        uint64_t fileSize = _file->tellg();
        _file->seekg(curPos, _file->beg);
        checkErr(_file);
        return fileSize;
    }


    void FileReadStream::seek(uint64_t pos) {
		if(!_file) {
			return;
		}

        _file->seekg(pos, _file->cur);
        checkErr(_file);
    }


    size_t FileReadStream::read(void *dst, size_t count) {
		if(!_file) {
			return 0;
		}

        uint64_t curPos = _file->tellg();
        _file->read((char*)dst, count);
        checkErr(_file);
        return (uint64_t)_file->tellg() - curPos;
    }



    void FileWriteStream::write(slice data) {
		if(_file) {
            uint64_t curPos = _file->tellg();
            _file->write((const char*)data.buf, data.size);
            if(((uint64_t)_file->tellg() - curPos) < data.size) {
                checkErr(_file);
            }
		}
    }

}
