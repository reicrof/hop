#ifndef IMDBG_H_
#define IMDBG_H_

#include <chrono>
#include <vector>

class ImProfiler;

void imProfInit();
ImProfiler* imNewProfiler( const char* );
void imProfNewFrame( int width, int height, int mouseX, int mouseY, bool leftMouseButtonPressed );
void imProfDraw();

class ImProfiler
{
   using Delta_t = float;

  public:
   struct ProfTrace
   {
      const char* name;
      std::chrono::time_point<std::chrono::system_clock> startTime;
      Delta_t* deltaTimeRef;
   };
   struct ProfRes
   {
      const char* name;
      Delta_t deltaTime;
      int level;
   };

   ImProfiler( ImProfiler&& ) = default;
   ImProfiler( const ImProfiler& ) = delete;
   ImProfiler& operator=( const ImProfiler& ) = delete;
   ImProfiler& operator=( ImProfiler&& ) = delete;

   void draw() const;
   void clear()
   {
      _tracesResults.clear();
      _traceStack.clear();
   }
   void pushProfTrace( const char* traceName );
   void popProfTrace();

  private:
   friend ImProfiler* imNewProfiler( const char* name );
   ImProfiler( const char* name );

   std::vector<ProfTrace> _traceStack;
   std::vector<ProfRes> _tracesResults;
   const char* _name;
   Delta_t* _curDelta;
   int currentLevel = {-1};
};

#endif  // IMDBG_H_
