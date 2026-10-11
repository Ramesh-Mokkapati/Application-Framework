#include "pch.h"

#include <cstdio>
#include <io.h>

// Replaces gtest_main (disabled in CoreTests.vcxproj) so the console stays open after the run.
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();

    // Only pause in an interactive console: Test Explorer, CI and piped runs redirect
    // stdin/stdout and would otherwise hang waiting for input
    if (_isatty(_fileno(stdin)) && _isatty(_fileno(stdout)))
    {
        printf("\nPress Enter to exit...\n");

        // stdout can be left fully buffered (e.g. after CaptureStdout() redirected it to a file),
        // and getchar() doesn't flush it, so the prompt would only appear after Enter
        fflush(stdout);
        getchar();
    }

    return result;
}
