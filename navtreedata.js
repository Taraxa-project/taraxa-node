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
    [ "Building taraxa-node", "md_doc_2building.html", [
      [ "Building on Ubuntu 24.04", "md_doc_2building.html#autotoc_md29", [
        [ "Compile-Time Options (cmake)", "md_doc_2building.html#autotoc_md27", [
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_2building.html#autotoc_md28", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_2building.html#autotoc_md30", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md31", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md32", null ]
      ] ],
      [ "Building on MacOS", "md_doc_2building.html#autotoc_md33", [
        [ "Install taraxa-node dependencies:", "md_doc_2building.html#autotoc_md34", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md35", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md36", null ],
        [ "Known issues", "md_doc_2building.html#autotoc_md37", [
          [ "Issues with conan cache", "md_doc_2building.html#autotoc_md38", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_2building.html#autotoc_md39", [
        [ "Install Rosetta2", "md_doc_2building.html#autotoc_md40", null ],
        [ "Run an x86_64 session", "md_doc_2building.html#autotoc_md41", null ],
        [ "Install Homebrew", "md_doc_2building.html#autotoc_md42", null ],
        [ "Install dependencies", "md_doc_2building.html#autotoc_md43", null ],
        [ "Clone the Repository", "md_doc_2building.html#autotoc_md44", null ],
        [ "Compile", "md_doc_2building.html#autotoc_md45", null ]
      ] ],
      [ "Run", "md_doc_2building.html#autotoc_md46", [
        [ "Running tests", "md_doc_2building.html#autotoc_md47", null ],
        [ "Running taraxa-node", "md_doc_2building.html#autotoc_md48", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_2building.html#autotoc_md49", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_2coding__practices.html", [
      [ "The structure of this document", "md_doc_2coding__practices.html#autotoc_md51", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_2coding__practices.html#autotoc_md52", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_2coding__practices.html#autotoc_md56", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_2coding__practices.html#autotoc_md63", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_2coding__practices.html#autotoc_md68", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_2coding__practices.html#autotoc_md77", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_2coding__practices.html#autotoc_md82", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_2coding__practices.html#autotoc_md86", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_2coding__practices.html#autotoc_md90", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_2coding__practices.html#autotoc_md95", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_2coding__practices.html#autotoc_md101", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_2coding__practices.html#autotoc_md106", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_2coding__practices.html#autotoc_md110", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_2coding__practices.html#autotoc_md114", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_2coding__practices.html#autotoc_md118", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_2coding__practices.html#autotoc_md124", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_2coding__practices.html#autotoc_md131", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_2coding__practices.html#autotoc_md136", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_2coding__practices.html#autotoc_md141", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_2coding__practices.html#autotoc_md146", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_2coding__practices.html#autotoc_md151", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_2coding__practices.html#autotoc_md162", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_2coding__practices.html#autotoc_md168", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_2coding__practices.html#autotoc_md172", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_2coding__practices.html#autotoc_md177", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_2coding__practices.html#autotoc_md181", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_2coding__practices.html#autotoc_md187", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_2coding__practices.html#autotoc_md194", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_2contributing.html", null ],
    [ "Doxygen", "md_doc_2doxygen.html", [
      [ "Installing dependencies", "md_doc_2doxygen.html#autotoc_md200", [
        [ "Ubuntu", "md_doc_2doxygen.html#autotoc_md201", null ],
        [ "MacOS", "md_doc_2doxygen.html#autotoc_md202", null ]
      ] ],
      [ "Generating documentation", "md_doc_2doxygen.html#autotoc_md203", null ],
      [ "Commenting", "md_doc_2doxygen.html#autotoc_md204", [
        [ "Basic example", "md_doc_2doxygen.html#autotoc_md205", null ],
        [ "Grouping to modules", "md_doc_2doxygen.html#autotoc_md206", null ],
        [ "Graphs", "md_doc_2doxygen.html#autotoc_md207", null ]
      ] ]
    ] ],
    [ "EVM incompatibilities", "md_doc_2evm__incompatibilities.html", [
      [ "Unsupported EIPs", "md_doc_2evm__incompatibilities.html#autotoc_md209", null ],
      [ "Latest supported solc version", "md_doc_2evm__incompatibilities.html#autotoc_md210", null ],
      [ "go-ethereum library", "md_doc_2evm__incompatibilities.html#autotoc_md211", null ],
      [ "Nonce handling", "md_doc_2evm__incompatibilities.html#autotoc_md212", null ]
    ] ],
    [ "Git-flow Guide", "md_doc_2git__practices.html", [
      [ "Branch naming conventions", "md_doc_2git__practices.html#autotoc_md213", null ],
      [ "Main branches", "md_doc_2git__practices.html#autotoc_md214", null ],
      [ "Supporting branches", "md_doc_2git__practices.html#autotoc_md215", [
        [ "Standard Feature branches", "md_doc_2git__practices.html#autotoc_md216", null ],
        [ "Long-term Feature branches", "md_doc_2git__practices.html#autotoc_md217", null ],
        [ "Hotfix branches", "md_doc_2git__practices.html#autotoc_md218", null ],
        [ "Release branches", "md_doc_2git__practices.html#autotoc_md219", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_2git__practices.html#autotoc_md220", null ],
      [ "PR merging & Code reviews", "md_doc_2git__practices.html#autotoc_md221", null ],
      [ "Commit message conventions", "md_doc_2git__practices.html#autotoc_md222", null ],
      [ "Automatic github issues linking", "md_doc_2git__practices.html#autotoc_md226", null ],
      [ "Example", "md_doc_2git__practices.html#autotoc_md227", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_2quickstart__guide.html", [
      [ "Taraxa docker image", "md_doc_2quickstart__guide.html#autotoc_md236", [
        [ "Pre-requisites", "md_doc_2quickstart__guide.html#autotoc_md229", [
          [ "MANDATORY PORT", "md_doc_2quickstart__guide.html#autotoc_md230", null ],
          [ "OPTIONAL PORTS", "md_doc_2quickstart__guide.html#autotoc_md231", null ]
        ] ],
        [ "Config", "md_doc_2quickstart__guide.html#autotoc_md232", [
          [ "Param1", "md_doc_2quickstart__guide.html#autotoc_md233", null ],
          [ "Param2", "md_doc_2quickstart__guide.html#autotoc_md234", null ],
          [ "...", "md_doc_2quickstart__guide.html#autotoc_md235", null ],
          [ "taraxa-builder:latest", "md_doc_2quickstart__guide.html#autotoc_md237", null ],
          [ "taraxa-node:latest", "md_doc_2quickstart__guide.html#autotoc_md238", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_2release__cycle.html", [
      [ "Release cycle phases", "md_doc_2release__cycle.html#autotoc_md240", [
        [ "Phase 1 - active development of new features", "md_doc_2release__cycle.html#autotoc_md241", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_2release__cycle.html#autotoc_md242", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_2release__cycle.html#autotoc_md243", null ],
        [ "Phase 4 - Mainnet release", "md_doc_2release__cycle.html#autotoc_md244", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_2release__cycle.html#autotoc_md245", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_2rewards__distribution.html", [
      [ "Glossary", "md_doc_2rewards__distribution.html#autotoc_md247", null ],
      [ "Rewards sources", "md_doc_2rewards__distribution.html#autotoc_md248", null ],
      [ "Rewards distribution", "md_doc_2rewards__distribution.html#autotoc_md249", [
        [ "Beneficial work in network", "md_doc_2rewards__distribution.html#autotoc_md250", null ],
        [ "Newly created tokens:", "md_doc_2rewards__distribution.html#autotoc_md251", null ],
        [ "Included transactions fees:", "md_doc_2rewards__distribution.html#autotoc_md252", null ]
      ] ],
      [ "Validators statistics", "md_doc_2rewards__distribution.html#autotoc_md253", null ],
      [ "Example:", "md_doc_2rewards__distribution.html#autotoc_md254", [
        [ "DAG structure:", "md_doc_2rewards__distribution.html#autotoc_md255", null ],
        [ "PBFT block", "md_doc_2rewards__distribution.html#autotoc_md256", null ],
        [ "Statistics", "md_doc_2rewards__distribution.html#autotoc_md257", null ],
        [ "Rewards", "md_doc_2rewards__distribution.html#autotoc_md258", [
          [ "DAG blocks rewards", "md_doc_2rewards__distribution.html#autotoc_md259", null ],
          [ "PBFT proposer reward", "md_doc_2rewards__distribution.html#autotoc_md260", null ],
          [ "PBFT voters reward", "md_doc_2rewards__distribution.html#autotoc_md261", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc_2_r_p_c.html", [
      [ "Ethereum compatibility", "md_doc_2_r_p_c.html#autotoc_md263", [
        [ "Quirks", "md_doc_2_r_p_c.html#autotoc_md264", null ],
        [ "Not implemented", "md_doc_2_r_p_c.html#autotoc_md265", null ],
        [ "eth_subscribe", "md_doc_2_r_p_c.html#autotoc_md266", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md267", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md268", null ]
        ] ],
        [ "Subscription types", "md_doc_2_r_p_c.html#autotoc_md269", null ],
        [ "newHeads", "md_doc_2_r_p_c.html#autotoc_md270", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md271", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md272", null ]
        ] ],
        [ "newPendingTransactions", "md_doc_2_r_p_c.html#autotoc_md273", [
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md274", null ]
        ] ],
        [ "newDagBlocks", "md_doc_2_r_p_c.html#autotoc_md275", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md276", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md277", null ]
        ] ],
        [ "logs", "md_doc_2_r_p_c.html#autotoc_md278", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md279", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md280", null ]
        ] ],
        [ "newDagBlocksFinalized", "md_doc_2_r_p_c.html#autotoc_md281", null ],
        [ "newPbftBlocks", "md_doc_2_r_p_c.html#autotoc_md282", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md283", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md284", null ]
        ] ],
        [ "newPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md285", [
          [ "params", "md_doc_2_r_p_c.html#autotoc_md286", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md287", null ]
        ] ]
      ] ],
      [ "Taraxa specific methods", "md_doc_2_r_p_c.html#autotoc_md288", [
        [ "taraxa_protocolVersion", "md_doc_2_r_p_c.html#autotoc_md289", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md290", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md291", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md292", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc_2_r_p_c.html#autotoc_md293", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md294", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md295", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md296", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc_2_r_p_c.html#autotoc_md297", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md298", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md299", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md300", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc_2_r_p_c.html#autotoc_md301", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md302", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md303", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md304", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc_2_r_p_c.html#autotoc_md305", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md306", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md307", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md308", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc_2_r_p_c.html#autotoc_md309", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md310", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md311", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md312", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc_2_r_p_c.html#autotoc_md313", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md314", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md315", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md316", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc_2_r_p_c.html#autotoc_md317", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md318", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md319", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md320", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc_2_r_p_c.html#autotoc_md321", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md322", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md323", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md324", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc_2_r_p_c.html#autotoc_md325", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md326", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md327", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md328", null ]
        ] ],
        [ "taraxa_yield", "md_doc_2_r_p_c.html#autotoc_md329", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md330", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md331", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md332", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc_2_r_p_c.html#autotoc_md333", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md334", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md335", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md336", null ]
        ] ],
        [ "taraxa_getPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md337", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md338", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md339", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md340", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc_2_r_p_c.html#autotoc_md341", [
        [ "get_sortition_change", "md_doc_2_r_p_c.html#autotoc_md342", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md343", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md344", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md345", null ]
        ] ],
        [ "send_coin_transaction", "md_doc_2_r_p_c.html#autotoc_md346", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md347", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md348", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md349", null ]
        ] ],
        [ "send_coin_transactions", "md_doc_2_r_p_c.html#autotoc_md350", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md351", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md352", null ]
        ] ],
        [ "get_account_address", "md_doc_2_r_p_c.html#autotoc_md353", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md354", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md355", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md356", null ]
        ] ],
        [ "get_peer_count", "md_doc_2_r_p_c.html#autotoc_md357", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md358", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md359", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md360", null ]
        ] ],
        [ "get_node_status", "md_doc_2_r_p_c.html#autotoc_md361", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md362", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md363", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md364", null ]
        ] ],
        [ "get_all_nodes", "md_doc_2_r_p_c.html#autotoc_md365", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md366", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md367", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md368", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc_2_r_p_c.html#autotoc_md369", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc_2_r_p_c.html#autotoc_md370", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md371", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md372", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md373", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc_2_r_p_c.html#autotoc_md374", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md375", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md376", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md377", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc_2_r_p_c.html#autotoc_md378", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md379", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md380", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md381", null ]
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
"_log_8h.html#aa7000f076a6a07cadb10faa23f141d15",
"class_modular_server.html#a85afd42d224c6e1d0e3d3436e6631606",
"classdev_1_1_r_l_p_stream.html#a7abc349151db3cc684b687d73ec99762",
"classdev_1_1p2p_1_1_node_table.html#ab246191ad4fefa9c29808ff63580445e",
"classgraphql_1_1taraxa_1_1_block.html#af34bf8cce518dc641bbc1f88fe25fb5c",
"classtaraxa_1_1_db_storage.html#a10e7214c85169ada7891f8722e956395",
"classtaraxa_1_1_network.html#a5c21890f89ed4e454378d7c6392a95e6",
"classtaraxa_1_1net_1_1_eth_client.html#a0705a9082cd73f627a8042f7197144d7",
"classtaraxa_1_1net_1_1_taraxa.html#a527aeb1708223067b08610c7a0afa587",
"classtaraxa_1_1net_1_1rpc_1_1eth_1_1_watches.html#a31b2b25b494ed5a73471dc15ee89f9dc",
"classtaraxa_1_1network_1_1tarcap_1_1_taraxa_capability.html#a3c10363f0276200813c50a2b12d9a096",
"classtaraxa_1_1rewards_1_1_block_stats.html#a92560300a1e3d60f67d472ab067fcc52",
"constants_8hpp.html#a5c4386d0ef4241fe4d08d14e43fbd1f6",
"functions_func_f.html",
"group___d_a_g.html#ab6d80eccedea603e03d4a194c5df0327",
"group___final_chain.html#abc6ac332556f6db92c462ce469b9ac8e",
"group___p_b_f_t.html#ae136c43ffd49e97a67186b2cf5719f91",
"group___vote.html#a27d9ac060898710e4ebeed9257a66b72",
"libdevcore_2_common_8h.html#abf4bb6315355a8af74bebe1be5187095",
"md_doc_2_r_p_c.html#autotoc_md322",
"namespacemembers_m.html",
"pbft__block_8cpp.html",
"structdev_1_1_static_log2.html",
"structdev_1_1p2p_1_1_pong.html#ae5e11949b02c2c655c2f73d01ceffc0d",
"structtaraxa_1_1_genesis_config.html#aa088067d31f2e40d0c6efd5b0298520e",
"taraxa-bootnode_2main_8cpp.html#a3c04138a5bfe5d72780bb7e82a18e627",
"watches_8hpp.html#acff653f7fb7bee54990e7ef3d29a7d87"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';