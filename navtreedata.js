/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "TARAXA", "index.html", [
    [ "Introducing Taraxa", "index.html", "index" ],
    [ "Multiple taraxa capabilities support", "md_libraries_2core__libs_2network_2src_2tarcap_2packets__handlers_2latest_2readme.html", null ],
    [ "Multiple taraxa capabilities support", "md_libraries_2core__libs_2network_2src_2tarcap_2packets__handlers_2v3_2readme.html", null ],
    [ "Building taraxa-node", "md_doc_2building.html", [
      [ "Building on Ubuntu 24.04", "md_doc_2building.html#autotoc_md30", [
        [ "Compile-Time Options (cmake)", "md_doc_2building.html#autotoc_md28", [
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_2building.html#autotoc_md29", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_2building.html#autotoc_md31", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md32", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md33", null ]
      ] ],
      [ "Building on MacOS", "md_doc_2building.html#autotoc_md34", [
        [ "Install taraxa-node dependencies:", "md_doc_2building.html#autotoc_md35", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md36", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md37", null ],
        [ "Known issues", "md_doc_2building.html#autotoc_md38", [
          [ "Issues with conan cache", "md_doc_2building.html#autotoc_md39", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_2building.html#autotoc_md40", [
        [ "Install Rosetta2", "md_doc_2building.html#autotoc_md41", null ],
        [ "Run an x86_64 session", "md_doc_2building.html#autotoc_md42", null ],
        [ "Install Homebrew", "md_doc_2building.html#autotoc_md43", null ],
        [ "Install dependencies", "md_doc_2building.html#autotoc_md44", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md45", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md46", null ]
      ] ],
      [ "Run", "md_doc_2building.html#autotoc_md47", [
        [ "Running tests", "md_doc_2building.html#autotoc_md48", null ],
        [ "Running taraxa-node", "md_doc_2building.html#autotoc_md49", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_2building.html#autotoc_md50", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_2coding__practices.html", [
      [ "The structure of this document", "md_doc_2coding__practices.html#autotoc_md52", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_2coding__practices.html#autotoc_md53", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_2coding__practices.html#autotoc_md57", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_2coding__practices.html#autotoc_md64", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_2coding__practices.html#autotoc_md69", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_2coding__practices.html#autotoc_md78", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_2coding__practices.html#autotoc_md83", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_2coding__practices.html#autotoc_md87", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_2coding__practices.html#autotoc_md91", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_2coding__practices.html#autotoc_md96", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_2coding__practices.html#autotoc_md102", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_2coding__practices.html#autotoc_md107", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_2coding__practices.html#autotoc_md111", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_2coding__practices.html#autotoc_md115", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_2coding__practices.html#autotoc_md119", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_2coding__practices.html#autotoc_md125", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_2coding__practices.html#autotoc_md132", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_2coding__practices.html#autotoc_md137", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_2coding__practices.html#autotoc_md142", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_2coding__practices.html#autotoc_md147", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_2coding__practices.html#autotoc_md152", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_2coding__practices.html#autotoc_md163", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_2coding__practices.html#autotoc_md169", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_2coding__practices.html#autotoc_md173", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_2coding__practices.html#autotoc_md178", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_2coding__practices.html#autotoc_md182", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_2coding__practices.html#autotoc_md188", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_2coding__practices.html#autotoc_md195", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_2contributing.html", null ],
    [ "Doxygen", "md_doc_2doxygen.html", [
      [ "Installing dependencies", "md_doc_2doxygen.html#autotoc_md201", [
        [ "Ubuntu", "md_doc_2doxygen.html#autotoc_md202", null ],
        [ "MacOS", "md_doc_2doxygen.html#autotoc_md203", null ]
      ] ],
      [ "Generating documentation", "md_doc_2doxygen.html#autotoc_md204", null ],
      [ "Commenting", "md_doc_2doxygen.html#autotoc_md205", [
        [ "Basic example", "md_doc_2doxygen.html#autotoc_md206", null ],
        [ "Grouping to modules", "md_doc_2doxygen.html#autotoc_md207", null ],
        [ "Graphs", "md_doc_2doxygen.html#autotoc_md208", null ]
      ] ]
    ] ],
    [ "EVM incompatibilities", "md_doc_2evm__incompatibilities.html", [
      [ "Unsupported EIPs", "md_doc_2evm__incompatibilities.html#autotoc_md210", null ],
      [ "Latest supported solc version", "md_doc_2evm__incompatibilities.html#autotoc_md211", null ],
      [ "go-ethereum library", "md_doc_2evm__incompatibilities.html#autotoc_md212", null ],
      [ "Nonce handling", "md_doc_2evm__incompatibilities.html#autotoc_md213", null ]
    ] ],
    [ "Git-flow Guide", "md_doc_2git__practices.html", [
      [ "Branch naming conventions", "md_doc_2git__practices.html#autotoc_md214", null ],
      [ "Main branches", "md_doc_2git__practices.html#autotoc_md215", null ],
      [ "Supporting branches", "md_doc_2git__practices.html#autotoc_md216", [
        [ "Standard Feature branches", "md_doc_2git__practices.html#autotoc_md217", null ],
        [ "Long-term Feature branches", "md_doc_2git__practices.html#autotoc_md218", null ],
        [ "Hotfix branches", "md_doc_2git__practices.html#autotoc_md219", null ],
        [ "Release branches", "md_doc_2git__practices.html#autotoc_md220", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_2git__practices.html#autotoc_md221", null ],
      [ "PR merging & Code reviews", "md_doc_2git__practices.html#autotoc_md222", null ],
      [ "Commit message conventions", "md_doc_2git__practices.html#autotoc_md223", null ],
      [ "Automatic github issues linking", "md_doc_2git__practices.html#autotoc_md227", null ],
      [ "Example", "md_doc_2git__practices.html#autotoc_md228", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_2quickstart__guide.html", [
      [ "Taraxa docker image", "md_doc_2quickstart__guide.html#autotoc_md237", [
        [ "Pre-requisites", "md_doc_2quickstart__guide.html#autotoc_md230", [
          [ "MANDATORY PORT", "md_doc_2quickstart__guide.html#autotoc_md231", null ],
          [ "OPTIONAL PORTS", "md_doc_2quickstart__guide.html#autotoc_md232", null ]
        ] ],
        [ "Config", "md_doc_2quickstart__guide.html#autotoc_md233", [
          [ "Param1", "md_doc_2quickstart__guide.html#autotoc_md234", null ],
          [ "Param2", "md_doc_2quickstart__guide.html#autotoc_md235", null ],
          [ "...", "md_doc_2quickstart__guide.html#autotoc_md236", null ],
          [ "taraxa-builder:latest", "md_doc_2quickstart__guide.html#autotoc_md238", null ],
          [ "taraxa-node:latest", "md_doc_2quickstart__guide.html#autotoc_md239", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_2release__cycle.html", [
      [ "Release cycle phases", "md_doc_2release__cycle.html#autotoc_md241", [
        [ "Phase 1 - active development of new features", "md_doc_2release__cycle.html#autotoc_md242", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_2release__cycle.html#autotoc_md243", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_2release__cycle.html#autotoc_md244", null ],
        [ "Phase 4 - Mainnet release", "md_doc_2release__cycle.html#autotoc_md245", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_2release__cycle.html#autotoc_md246", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_2rewards__distribution.html", [
      [ "Glossary", "md_doc_2rewards__distribution.html#autotoc_md248", null ],
      [ "Rewards sources", "md_doc_2rewards__distribution.html#autotoc_md249", null ],
      [ "Rewards distribution", "md_doc_2rewards__distribution.html#autotoc_md250", [
        [ "Beneficial work in network", "md_doc_2rewards__distribution.html#autotoc_md251", null ],
        [ "Newly created tokens:", "md_doc_2rewards__distribution.html#autotoc_md252", null ],
        [ "Included transactions fees:", "md_doc_2rewards__distribution.html#autotoc_md253", null ]
      ] ],
      [ "Validators statistics", "md_doc_2rewards__distribution.html#autotoc_md254", null ],
      [ "Example:", "md_doc_2rewards__distribution.html#autotoc_md255", [
        [ "DAG structure:", "md_doc_2rewards__distribution.html#autotoc_md256", null ],
        [ "PBFT block", "md_doc_2rewards__distribution.html#autotoc_md257", null ],
        [ "Statistics", "md_doc_2rewards__distribution.html#autotoc_md258", null ],
        [ "Rewards", "md_doc_2rewards__distribution.html#autotoc_md259", [
          [ "DAG blocks rewards", "md_doc_2rewards__distribution.html#autotoc_md260", null ],
          [ "PBFT proposer reward", "md_doc_2rewards__distribution.html#autotoc_md261", null ],
          [ "PBFT voters reward", "md_doc_2rewards__distribution.html#autotoc_md262", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc_2_r_p_c.html", [
      [ "Ethereum compatibility", "md_doc_2_r_p_c.html#autotoc_md264", [
        [ "Quirks", "md_doc_2_r_p_c.html#autotoc_md265", null ],
        [ "Not implemented", "md_doc_2_r_p_c.html#autotoc_md266", null ],
        [ "eth_subscribe", "md_doc_2_r_p_c.html#autotoc_md267", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md268", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md269", null ]
        ] ],
        [ "Subscription types", "md_doc_2_r_p_c.html#autotoc_md270", null ],
        [ "newHeads", "md_doc_2_r_p_c.html#autotoc_md271", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md272", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md273", null ]
        ] ],
        [ "newPendingTransactions", "md_doc_2_r_p_c.html#autotoc_md274", [
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md275", null ]
        ] ],
        [ "newDagBlocks", "md_doc_2_r_p_c.html#autotoc_md276", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md277", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md278", null ]
        ] ],
        [ "logs", "md_doc_2_r_p_c.html#autotoc_md279", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md280", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md281", null ]
        ] ],
        [ "newDagBlocksFinalized", "md_doc_2_r_p_c.html#autotoc_md282", null ],
        [ "newPbftBlocks", "md_doc_2_r_p_c.html#autotoc_md283", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md284", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md285", null ]
        ] ],
        [ "newPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md286", [
          [ "params", "md_doc_2_r_p_c.html#autotoc_md287", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md288", null ]
        ] ]
      ] ],
      [ "Taraxa specific methods", "md_doc_2_r_p_c.html#autotoc_md289", [
        [ "taraxa_protocolVersion", "md_doc_2_r_p_c.html#autotoc_md290", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md291", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md292", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md293", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc_2_r_p_c.html#autotoc_md294", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md295", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md296", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md297", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc_2_r_p_c.html#autotoc_md298", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md299", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md300", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md301", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc_2_r_p_c.html#autotoc_md302", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md303", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md304", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md305", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc_2_r_p_c.html#autotoc_md306", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md307", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md308", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md309", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc_2_r_p_c.html#autotoc_md310", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md311", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md312", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md313", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc_2_r_p_c.html#autotoc_md314", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md315", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md316", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md317", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc_2_r_p_c.html#autotoc_md318", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md319", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md320", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md321", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc_2_r_p_c.html#autotoc_md322", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md323", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md324", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md325", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc_2_r_p_c.html#autotoc_md326", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md327", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md328", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md329", null ]
        ] ],
        [ "taraxa_yield", "md_doc_2_r_p_c.html#autotoc_md330", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md331", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md332", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md333", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc_2_r_p_c.html#autotoc_md334", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md335", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md336", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md337", null ]
        ] ],
        [ "taraxa_getPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md338", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md339", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md340", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md341", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc_2_r_p_c.html#autotoc_md342", [
        [ "get_sortition_change", "md_doc_2_r_p_c.html#autotoc_md343", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md344", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md345", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md346", null ]
        ] ],
        [ "send_coin_transaction", "md_doc_2_r_p_c.html#autotoc_md347", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md348", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md349", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md350", null ]
        ] ],
        [ "send_coin_transactions", "md_doc_2_r_p_c.html#autotoc_md351", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md352", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md353", null ]
        ] ],
        [ "get_account_address", "md_doc_2_r_p_c.html#autotoc_md354", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md355", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md356", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md357", null ]
        ] ],
        [ "get_peer_count", "md_doc_2_r_p_c.html#autotoc_md358", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md359", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md360", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md361", null ]
        ] ],
        [ "get_node_status", "md_doc_2_r_p_c.html#autotoc_md362", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md363", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md364", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md365", null ]
        ] ],
        [ "get_all_nodes", "md_doc_2_r_p_c.html#autotoc_md366", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md367", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md368", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md369", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc_2_r_p_c.html#autotoc_md370", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc_2_r_p_c.html#autotoc_md371", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md372", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md373", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md374", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc_2_r_p_c.html#autotoc_md375", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md376", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md377", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md378", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc_2_r_p_c.html#autotoc_md379", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md380", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md381", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md382", null ]
        ] ]
      ] ]
    ] ],
    [ "readme", "md_doc_2uml_2readme.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Topics", "topics.html", "topics" ],
    [ "Namespace Members", "namespacemembers.html", [
      [ "All", "namespacemembers.html", "namespacemembers_dup" ],
      [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
      [ "Variables", "namespacemembers_vars.html", null ],
      [ "Typedefs", "namespacemembers_type.html", null ],
      [ "Enumerations", "namespacemembers_enum.html", null ],
      [ "Enumerator", "namespacemembers_eval.html", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_a_e_s_8cpp.html",
"_log_8h.html#aab0ddabcbd87d29780f1da025e82a251",
"class_modular_server.html#ae1e7bfbc69320a618a2ae07251581334",
"classdev_1_1_r_l_p_stream.html#a9bcad2caa5234b8c3c5f088f373046bf",
"classdev_1_1p2p_1_1_node_table.html#acc79fb500db389c4480534a9c316e8ee",
"classgraphql_1_1taraxa_1_1_current_state.html#a9e0084c43ffe85bbdb236b8b0deee7b9",
"classtaraxa_1_1_db_storage.html#a7c5b83f06af69153fbb86037ecda6a1f",
"classtaraxa_1_1_proposed_blocks.html#a1b221518bae7f5641dc39e6e919ce681",
"classtaraxa_1_1net_1_1_eth_client.html#a7b082f0cf22b8ceaad6f1d62ae0bfcf7",
"classtaraxa_1_1net_1_1_taraxa_client.html",
"classtaraxa_1_1network_1_1tarcap_1_1_base_packet_handler.html#a5abf0fca90342c72c93d2c71f95b4cca",
"classtaraxa_1_1network_1_1tarcap_1_1_taraxa_capability.html#adacb5e77df8056f4088564e2c39d5a38",
"classtaraxa_1_1network_1_1tarcap_1_1v3_1_1_votes_bundle_packet_handler.html#a2145a984e2f67e8639d7d05b6709e2d4",
"classtaraxa_1_1util_1_1event_1_1_event_subscriber_1_1_state.html#af2d69ab22cb1b39a79b5851e79b31ec4",
"dir_89fb9f2660c424db234cfd81b0e055c3.html",
"group___d_a_g.html#a28322d856a13e424d0f4aa93a25690f3",
"group___final_chain.html#a5022eba73f454bf6a794b85c68320c74",
"group___p_b_f_t.html#a60b3092152bc8fc64ab68638ef6f39aa",
"group___transaction.html#a50f1bb894c6ebf89cff9d80a238d1492",
"hardfork_8hpp.html#a38c082406284e8f00e5cd7250a1fef7e",
"libp2p_2_common_8h.html#a1a6ed6078a9c0a52b98d376eb09397be",
"md_doc_2coding__practices.html#autotoc_md57",
"namespacetaraxa_1_1network_1_1tarcap.html#a385696bb1ace4285f2b3f5e1cb2175c1",
"state__api__data_8cpp.html",
"structdev_1_1p2p_1_1_host.html#ab8336e770280ba43700db54b3020e2f5",
"structdev_1_1p2p_1_1_session.html#a9f81114e13214a9c93614252f2cf24e3",
"structtaraxa_1_1_transaction.html#afc548040d0786be0722136aa94d4298a",
"types_8hpp.html#a246ff1eeae4d7fef0f54e7384b812bf0"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';