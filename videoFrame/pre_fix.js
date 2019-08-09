var __ffmpegjs_utf8ToStr;
var __ffmpegjs_opts = {};
var __ffmpegjs_return = false;
var seek_video_to = null;
var isFSbuild = false ;
var mountpoint = undefined ;    // all the mountpoint should be the same in imagination

function __ffmpegjs_toU8(data) {
    if (Array.isArray(data) || data instanceof ArrayBuffer) {
        data = new Uint8Array(data);
    } else if (!data) {
        // `null` for empty files.
        data = new Uint8Array(0);
    } else if (!(data instanceof Uint8Array)) {
        // Avoid unnecessary copying.
        data = new Uint8Array(data.buffer);
    }
    return data;
}

function __preRun(){
    (__ffmpegjs_opts["mounts"] || []).forEach(function(mount) {
        var fs = FS.filesystems[mount["type"]];
        if (!fs) {
            throw new Error("Bad mount type");
        }
        mountpoint = mount["mountpoint"];
        // NOTE(Kagami): Subdirs are not allowed in the paths to simplify
        // things and avoid ".." escapes.
        if (!mountpoint.match(/^\/[^\/]+$/) ||
            mountpoint === "/." ||
            mountpoint === "/.." ||
            mountpoint === "/tmp" ||
            mountpoint === "/home" ||
            mountpoint === "/dev" ||
            mountpoint === "/work") {
            throw new Error("Bad mount point");
        }

        
       
        FS.mkdir(mountpoint);
        FS.mount(fs, mount["opts"], mountpoint);
        console.log('finish mount')
    });
    if(!isFSbuild){
        FS.mkdir("/work");
        FS.chdir("/work"); 
        isFSbuild = true; 
    }
    
}

function __postRun(){
    // NOTE(Kagami): Search for files only in working directory, one
      // level depth. Since FFmpeg shouldn't normally create
      // subdirectories, it should be enough.
      function listFiles(dir) {
        var contents = FS.lookupPath(dir).node.contents;
        var filenames = Object.keys(contents);
        // Fix for possible file with "__proto__" name. See
        // <https://github.com/kripken/emscripten/issues/3663> for
        // details.
        if (contents.__proto__ && contents.__proto__.name === "__proto__") {
            filenames.push("__proto__");
        }
        return filenames.map(function(filename) {
            return contents[filename];
        });
    }

    var inFiles = Object.create(null);
    (__ffmpegjs_opts["MEMFS"] || []).forEach(function(file) {
        inFiles[file.name] = null;
    });
    var outFiles = listFiles("/work").filter(function(file) {
        return !(file.name in inFiles);
    }).map(function(file) {
        var data = __ffmpegjs_toU8(file.contents);
        return {"name": file.name, "data": data};
    });
    __ffmpegjs_return = {"MEMFS": outFiles};
    FS.unmount(mountpoint);
    FS.rmdir(mountpoint);
    // FS.unlink("/data");
    if (Module['returnCallback']) Module['returnCallback'](__ffmpegjs_return);
}

function __ffmpegjs(__ffmpegjs_opts1) {
    __ffmpegjs_utf8ToStr = UTF8ArrayToString;
    __ffmpegjs_opts = __ffmpegjs_opts1 || {};
    Object.keys(__ffmpegjs_opts).forEach(function(key) {
        if (key != "mounts" && key != "MEMFS") {
            Module[key] = __ffmpegjs_opts[key];
        }
    });

    // 挂载文件到fs
    __preRun();
    // 执行exported function
    seek_video_to(__ffmpegjs_opts["arguments"][0], __ffmpegjs_opts["arguments"][1]);
    // 取文件返回
    __postRun();
    // Object.keys(__ffmpegjs_opts).forEach(function(key) {
    //     if (key != "mounts" && key != "MEMFS") {
    //         Module[key] = __ffmpegjs_opts[key];
    //     }
    // });
    // to preload the wasm file
    return __ffmpegjs_return;
}    

Module['locateFile'] = function(path, prefix) {
    // otherwise, use the default, the prefix (JS file's dir) + the path
        console.log('path'+ path + '\n')
      return prefix + path;
  };
  Module.onRuntimeInitialized = function () {
    seek_video_to = Module.cwrap('seekseek', 'number', ['string', 'number']);
  };

  


