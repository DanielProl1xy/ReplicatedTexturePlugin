# Replicated Texture Plugin for Unreal Engine
This plugin provides simple API for replicating texture along the networked players.

## Overview
![blueprint overview](/Resources/View.png)

## How to use
To use, add component "ReplicatedTexture" to player controller and pass Texture2D with associated name to node "ReplicateTexture". To access the replicated texture - use either OnTextureReady delegate on another machine, either try to look for it using "FindTexture" function.

### Attention
Replicating textures is quite heavy operation. It loads bandwidth hard enough, so be aware to use it properly, otherwise you're gonna have performace issues.

This plugin uses built-in PNG compression. All compression/decompression operations are done in asynchronous style, to optimize performance.
