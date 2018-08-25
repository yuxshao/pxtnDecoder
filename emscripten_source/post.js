// ENVIRONMENT
Module["ENVIRONMENT"] = ENVIRONMENT_IS_WEB ? "WEB" :
                        ENVIRONMENT_IS_WORKER ? "WORKER" :
                        ENVIRONMENT_IS_NODE ? "NODE" : "SHELL";

Module["getNativeTypeSize"] = getNativeTypeSize;

Module["waitForReady"] = new Promise(function (resolve, _) {
  Module["onRuntimeInitialized"] = resolve;
});

// export for browserify
if(!ENVIRONMENT_IS_NODE && typeof module !== 'undefined')
    module["exports"] = Module;
