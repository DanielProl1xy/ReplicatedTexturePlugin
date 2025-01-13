# Replicated Texture Plugin for Unreal Engine
This plugin provides simple API for replicating texture along the networked players.

## Overview
![blueprint overview](/Resources/View.png)

### Attention
Replicating textures is quite heavy operation. It loads bandwidth hard enough, so be aware to use it properly, otherwise you're gonna have performace issues.

This plugin uses built-in PNG compression. All compression/decompression operations are done in asynchronous style, to optimize performance.
