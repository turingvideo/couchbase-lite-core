//
// PrebuiltCopier.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#include "PrebuiltCopier.hh"
#include "Logging.hh"
#include "FilePath.hh"
#include "Database.hh"
#include "StringUtil.hh"
#include "Error.hh"
#include "c4Database.h"
#include <future>

namespace litecore {
    using namespace std;
    
    void CopyPrebuiltDB(const fs::path &from, const fs::path &to,
                             const C4DatabaseConfig *config) {
        if(!fs::exists(from)) {
            Warn("No database exists at %s, cannot copy!", from.c_str());
            error::_throw(error::Domain::LiteCore, kC4ErrorNotFound);
        }

        if (fs::exists(to)) {
            Warn("Database already exists at %s, cannot copy!", to.c_str());
            error::_throw(error::Domain::POSIX, EEXIST);
        }
        
        fs::path backupPath;
        Log("Copying prebuilt database from %s to %s", from.c_str(), to.c_str());

        char dir_buffer[] = "XXXXXX";
        fs::path temp = fs::temp_directory_path() / temp_filename(dir_buffer);
        fs::remove_all(temp);
        fs::copy(from, temp);
        
        auto db = make_unique<C4Database>(temp, *config);
        db->resetUUIDs();
        db->close();
        
        try {
            Log("Moving source DB to destination DB...");
            fs::rename(temp, to);
        } catch(...) {
            Warn("Failed to finish copying database");
            fs::remove_all(to);
            throw;
        }
    }
}
