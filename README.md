# static_pathcch
PathCch.h implementation with no Windows 8+ dependency

Most of these functions haven't been properly tested and probably contain bugs. Use with caution.

## PathCch???
https://docs.microsoft.com/en-us/windows/desktop/api/pathcch/

## How complete is this?
Everything is done except for `PathCchCanonicalizeEx`, `PathCchCombineEx` and the functions that call into them.

## How do I use this?
Add `pathcch.c` to your solution, `#include <PathCch.h>` as you normally would, and make sure you aren't linking to `Pathcch.lib`. That's it.

## How can I contribute?
Unit tests that compare results with the official Microsoft implementation would be more than welcome.
