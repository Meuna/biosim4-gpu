# Design principles

The project is developed following an AI first approach:

1. First, I used Claude Code agent to analyze the data model powering the
   behavioral neural network. I asked for a design document, with an emphasis on
   foreseen GPU constraints (e.g. some behaviors depends on neighbors
   characteristics, behind 2 pointers on the heap).
2. I then iterated with the claude.ai chat using the Opus 4.7 to design a new,
   GPU friendly data model and a repository structure.
3. The next steps will be clarified as they arrive.
