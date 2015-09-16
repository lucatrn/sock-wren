#include <stdio.h>
#include <string.h>

#include "modules.h"
#include "scheduler.h"
#include "vm.h"

#define MAX_LINE_LENGTH 1024 // TODO: Something less arbitrary.

// The single VM instance that the CLI uses.
static WrenVM* vm;

static WrenBindForeignMethodFn bindMethodFn = NULL;
static WrenBindForeignClassFn bindClassFn = NULL;

static uv_loop_t* loop;

static char const* rootDirectory = NULL;

// Reads the contents of the file at [path] and returns it as a heap allocated
// string.
//
// Returns `NULL` if the path could not be found. Exits if it was found but
// could not be read.
static char* readFile(const char* path)
{
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;
  
  // Find out how big the file is.
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  
  // Allocate a buffer for it.
  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL)
  {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  
  // Read the entire file.
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize)
  {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  
  // Terminate the string.
  buffer[bytesRead] = '\0';
  
  fclose(file);
  return buffer;
}

// Converts the module [name] to a file path.
static char* wrenFilePath(const char* name)
{
  // The module path is relative to the root directory and with ".wren".
  size_t rootLength = rootDirectory == NULL ? 0 : strlen(rootDirectory);
  size_t nameLength = strlen(name);
  size_t pathLength = rootLength + nameLength + 5;
  char* path = (char*)malloc(pathLength + 1);
  
  if (rootDirectory != NULL)
  {
    memcpy(path, rootDirectory, rootLength);
  }
  
  memcpy(path + rootLength, name, nameLength);
  memcpy(path + rootLength + nameLength, ".wren", 5);
  path[pathLength] = '\0';
  
  return path;
}

// Attempts to read the source for [module] relative to the current root
// directory.
//
// Returns it if found, or NULL if the module could not be found. Exits if the
// module was found but could not be read.
static char* readModule(WrenVM* vm, const char* module)
{
  char* source = readBuiltInModule(module);
  if (source != NULL) return source;
  
  // First try to load the module with a ".wren" extension.
  char* modulePath = wrenFilePath(module);
  char* moduleContents = readFile(modulePath);
  free(modulePath);
  
  if (moduleContents != NULL) return moduleContents;
  
  // If no contents could be loaded treat the module name as specifying a
  // directory and try to load the "module.wren" file in the directory.
  size_t moduleLength = strlen(module);
  size_t moduleDirLength = moduleLength + 7;
  char* moduleDir = (char*)malloc(moduleDirLength + 1);
  memcpy(moduleDir, module, moduleLength);
  memcpy(moduleDir + moduleLength, "/module", 7);
  moduleDir[moduleDirLength] = '\0';
  
  char* moduleDirPath = wrenFilePath(moduleDir);
  free(moduleDir);
  
  moduleContents = readFile(moduleDirPath);
  free(moduleDirPath);
  
  return moduleContents;
}

// Binds foreign methods declared in either built in modules, or the injected
// API test modules.
static WrenForeignMethodFn bindForeignMethod(WrenVM* vm, const char* module,
    const char* className, bool isStatic, const char* signature)
{
  WrenForeignMethodFn method = bindBuiltInForeignMethod(vm, module, className,
                                                        isStatic, signature);
  if (method != NULL) return method;
  
  if (bindMethodFn != NULL)
  {
    return bindMethodFn(vm, module, className, isStatic, signature);
  }

  return NULL;
}

// Binds foreign classes declared in either built in modules, or the injected
// API test modules.
static WrenForeignClassMethods bindForeignClass(
    WrenVM* vm, const char* module, const char* className)
{
  WrenForeignClassMethods methods = bindBuiltInForeignClass(vm, module,
                                                            className);
  if (methods.allocate != NULL) return methods;

  if (bindClassFn != NULL)
  {
    return bindClassFn(vm, module, className);
  }

  return methods;
}

static void write(WrenVM* vm, const char* text)
{
  printf("%s", text);
}

static void initVM()
{
  WrenConfiguration config;
  wrenInitConfiguration(&config);

  config.bindForeignMethodFn = bindForeignMethod;
  config.bindForeignClassFn = bindForeignClass;
  config.loadModuleFn = readModule;
  config.writeFn = write;

  // Since we're running in a standalone process, be generous with memory.
  config.initialHeapSize = 1024 * 1024 * 100;
  vm = wrenNewVM(&config);

  // Initialize the event loop.
  loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
  uv_loop_init(loop);
}

static void freeVM()
{
  schedulerReleaseMethods();

  uv_loop_close(loop);
  free(loop);

  wrenFreeVM(vm);
}

void runFile(const char* path)
{
  // Use the directory where the file is as the root to resolve imports
  // relative to.
  char* root = NULL;
  const char* lastSlash = strrchr(path, '/');
  if (lastSlash != NULL)
  {
    root = (char*)malloc(lastSlash - path + 2);
    memcpy(root, path, lastSlash - path + 1);
    root[lastSlash - path + 1] = '\0';
    rootDirectory = root;
  }

  char* source = readFile(path);
  if (source == NULL)
  {
    fprintf(stderr, "Could not find file \"%s\".\n", path);
    exit(66);
  }

  initVM();

  WrenInterpretResult result = wrenInterpret(vm, path, source);

  if (result == WREN_RESULT_SUCCESS)
  {
    uv_run(loop, UV_RUN_DEFAULT);
  }

  freeVM();

  free(source);
  free(root);

  // Exit with an error code if the script failed.
  if (result == WREN_RESULT_COMPILE_ERROR) exit(65); // EX_DATAERR.
  if (result == WREN_RESULT_RUNTIME_ERROR) exit(70); // EX_SOFTWARE.
}

int runRepl()
{
  initVM();

  printf("\\\\/\"-\n");
  printf(" \\_/   wren v0.0.0\n");

  char line[MAX_LINE_LENGTH];

  for (;;)
  {
    printf("> ");

    if (!fgets(line, MAX_LINE_LENGTH, stdin))
    {
      printf("\n");
      break;
    }

    // TODO: Handle failure.
    wrenInterpret(vm, "Prompt", line);

    // TODO: Automatically print the result of expressions.
  }

  freeVM();

  return 0;
}

WrenVM* getVM()
{
  return vm;
}

uv_loop_t* getLoop()
{
  return loop;
}

void setForeignCallbacks(
    WrenBindForeignMethodFn bindMethod, WrenBindForeignClassFn bindClass)
{
  bindMethodFn = bindMethod;
  bindClassFn = bindClass;
}