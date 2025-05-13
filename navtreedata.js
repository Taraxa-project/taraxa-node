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
      [ "Building project", "md_doc_2building.html#autotoc_md29", [
        [ "Compile-Time Options (cmake)", "md_doc_2building.html#autotoc_md27", [
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_2building.html#autotoc_md28", null ]
        ] ],
        [ "1. Install dependencies", "md_doc_2building.html#autotoc_md30", [
          [ "[Ubuntu 24.04]", "md_doc_2building.html#autotoc_md31", null ]
        ] ]
      ] ],
      [ "Required packages", "md_doc_2building.html#autotoc_md32", null ],
      [ "Optional. Needed to run py_test. This won't install on arm64 OS because package is missing in apt", "md_doc_2building.html#autotoc_md33", null ],
      [ "Install conan package manager", "md_doc_2building.html#autotoc_md34", [
        [ "Run", "md_doc_2building.html#autotoc_md40", [
          [ "2. Clone the Repository", "md_doc_2building.html#autotoc_md36", null ],
          [ "3. Compile", "md_doc_2building.html#autotoc_md37", null ],
          [ "Known issues", "md_doc_2building.html#autotoc_md38", [
            [ "[MacOS]", "md_doc_2building.html#autotoc_md35", null ],
            [ "Issues with conan cache", "md_doc_2building.html#autotoc_md39", null ]
          ] ],
          [ "Running tests", "md_doc_2building.html#autotoc_md41", null ],
          [ "Running taraxa-node", "md_doc_2building.html#autotoc_md42", null ]
        ] ]
      ] ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_2coding__practices.html", [
      [ "The structure of this document", "md_doc_2coding__practices.html#autotoc_md44", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_2coding__practices.html#autotoc_md45", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_2coding__practices.html#autotoc_md49", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_2coding__practices.html#autotoc_md56", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_2coding__practices.html#autotoc_md61", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_2coding__practices.html#autotoc_md70", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_2coding__practices.html#autotoc_md75", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_2coding__practices.html#autotoc_md79", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_2coding__practices.html#autotoc_md83", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_2coding__practices.html#autotoc_md88", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_2coding__practices.html#autotoc_md94", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_2coding__practices.html#autotoc_md99", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_2coding__practices.html#autotoc_md103", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_2coding__practices.html#autotoc_md107", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_2coding__practices.html#autotoc_md111", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_2coding__practices.html#autotoc_md117", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_2coding__practices.html#autotoc_md124", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_2coding__practices.html#autotoc_md129", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_2coding__practices.html#autotoc_md134", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_2coding__practices.html#autotoc_md139", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_2coding__practices.html#autotoc_md144", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_2coding__practices.html#autotoc_md155", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_2coding__practices.html#autotoc_md161", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_2coding__practices.html#autotoc_md165", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_2coding__practices.html#autotoc_md170", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_2coding__practices.html#autotoc_md174", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_2coding__practices.html#autotoc_md180", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_2coding__practices.html#autotoc_md187", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_2contributing.html", null ],
    [ "Doxygen", "md_doc_2doxygen.html", [
      [ "Installing dependencies", "md_doc_2doxygen.html#autotoc_md193", [
        [ "Ubuntu", "md_doc_2doxygen.html#autotoc_md194", null ],
        [ "MacOS", "md_doc_2doxygen.html#autotoc_md195", null ]
      ] ],
      [ "Generating documentation", "md_doc_2doxygen.html#autotoc_md196", null ],
      [ "Commenting", "md_doc_2doxygen.html#autotoc_md197", [
        [ "Basic example", "md_doc_2doxygen.html#autotoc_md198", null ],
        [ "Grouping to modules", "md_doc_2doxygen.html#autotoc_md199", null ],
        [ "Graphs", "md_doc_2doxygen.html#autotoc_md200", null ]
      ] ]
    ] ],
    [ "EVM incompatibilities", "md_doc_2evm__incompatibilities.html", [
      [ "Unsupported EIPs", "md_doc_2evm__incompatibilities.html#autotoc_md202", null ],
      [ "Latest supported solc version", "md_doc_2evm__incompatibilities.html#autotoc_md203", null ],
      [ "go-ethereum library", "md_doc_2evm__incompatibilities.html#autotoc_md204", null ],
      [ "Nonce handling", "md_doc_2evm__incompatibilities.html#autotoc_md205", null ]
    ] ],
    [ "Git-flow Guide", "md_doc_2git__practices.html", [
      [ "Branch naming conventions", "md_doc_2git__practices.html#autotoc_md206", null ],
      [ "Main branches", "md_doc_2git__practices.html#autotoc_md207", null ],
      [ "Supporting branches", "md_doc_2git__practices.html#autotoc_md208", [
        [ "Standard Feature branches", "md_doc_2git__practices.html#autotoc_md209", null ],
        [ "Long-term Feature branches", "md_doc_2git__practices.html#autotoc_md210", null ],
        [ "Hotfix branches", "md_doc_2git__practices.html#autotoc_md211", null ],
        [ "Release branches", "md_doc_2git__practices.html#autotoc_md212", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_2git__practices.html#autotoc_md213", null ],
      [ "PR merging & Code reviews", "md_doc_2git__practices.html#autotoc_md214", null ],
      [ "Commit message conventions", "md_doc_2git__practices.html#autotoc_md215", null ],
      [ "Automatic github issues linking", "md_doc_2git__practices.html#autotoc_md219", null ],
      [ "Example", "md_doc_2git__practices.html#autotoc_md220", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_2quickstart__guide.html", [
      [ "Taraxa docker image", "md_doc_2quickstart__guide.html#autotoc_md229", [
        [ "Pre-requisites", "md_doc_2quickstart__guide.html#autotoc_md222", [
          [ "MANDATORY PORT", "md_doc_2quickstart__guide.html#autotoc_md223", null ],
          [ "OPTIONAL PORTS", "md_doc_2quickstart__guide.html#autotoc_md224", null ]
        ] ],
        [ "Config", "md_doc_2quickstart__guide.html#autotoc_md225", [
          [ "Param1", "md_doc_2quickstart__guide.html#autotoc_md226", null ],
          [ "Param2", "md_doc_2quickstart__guide.html#autotoc_md227", null ],
          [ "...", "md_doc_2quickstart__guide.html#autotoc_md228", null ],
          [ "taraxa-builder:latest", "md_doc_2quickstart__guide.html#autotoc_md230", null ],
          [ "taraxa-node:latest", "md_doc_2quickstart__guide.html#autotoc_md231", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_2release__cycle.html", [
      [ "Release cycle phases", "md_doc_2release__cycle.html#autotoc_md233", [
        [ "Phase 1 - active development of new features", "md_doc_2release__cycle.html#autotoc_md234", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_2release__cycle.html#autotoc_md235", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_2release__cycle.html#autotoc_md236", null ],
        [ "Phase 4 - Mainnet release", "md_doc_2release__cycle.html#autotoc_md237", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_2release__cycle.html#autotoc_md238", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_2rewards__distribution.html", [
      [ "Glossary", "md_doc_2rewards__distribution.html#autotoc_md240", null ],
      [ "Rewards sources", "md_doc_2rewards__distribution.html#autotoc_md241", null ],
      [ "Rewards distribution", "md_doc_2rewards__distribution.html#autotoc_md242", [
        [ "Beneficial work in network", "md_doc_2rewards__distribution.html#autotoc_md243", null ],
        [ "Newly created tokens:", "md_doc_2rewards__distribution.html#autotoc_md244", null ],
        [ "Included transactions fees:", "md_doc_2rewards__distribution.html#autotoc_md245", null ]
      ] ],
      [ "Validators statistics", "md_doc_2rewards__distribution.html#autotoc_md246", null ],
      [ "Example:", "md_doc_2rewards__distribution.html#autotoc_md247", [
        [ "DAG structure:", "md_doc_2rewards__distribution.html#autotoc_md248", null ],
        [ "PBFT block", "md_doc_2rewards__distribution.html#autotoc_md249", null ],
        [ "Statistics", "md_doc_2rewards__distribution.html#autotoc_md250", null ],
        [ "Rewards", "md_doc_2rewards__distribution.html#autotoc_md251", [
          [ "DAG blocks rewards", "md_doc_2rewards__distribution.html#autotoc_md252", null ],
          [ "PBFT proposer reward", "md_doc_2rewards__distribution.html#autotoc_md253", null ],
          [ "PBFT voters reward", "md_doc_2rewards__distribution.html#autotoc_md254", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc_2_r_p_c.html", [
      [ "Ethereum compatibility", "md_doc_2_r_p_c.html#autotoc_md256", [
        [ "Quirks", "md_doc_2_r_p_c.html#autotoc_md257", null ],
        [ "Not implemented", "md_doc_2_r_p_c.html#autotoc_md258", null ],
        [ "eth_subscribe", "md_doc_2_r_p_c.html#autotoc_md259", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md260", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md261", null ]
        ] ],
        [ "Subscription types", "md_doc_2_r_p_c.html#autotoc_md262", null ],
        [ "newHeads", "md_doc_2_r_p_c.html#autotoc_md263", [
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md264", null ]
        ] ],
        [ "newPendingTransactions", "md_doc_2_r_p_c.html#autotoc_md265", [
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md266", null ]
        ] ],
        [ "newDagBlocks", "md_doc_2_r_p_c.html#autotoc_md267", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md268", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md269", null ]
        ] ],
        [ "logs", "md_doc_2_r_p_c.html#autotoc_md270", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md271", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md272", null ]
        ] ],
        [ "newDagBlocksFinalized", "md_doc_2_r_p_c.html#autotoc_md273", null ],
        [ "newPbftBlocks", "md_doc_2_r_p_c.html#autotoc_md274", [
          [ "Params", "md_doc_2_r_p_c.html#autotoc_md275", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md276", null ]
        ] ],
        [ "newPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md277", [
          [ "params", "md_doc_2_r_p_c.html#autotoc_md278", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md279", null ]
        ] ]
      ] ],
      [ "Taraxa specific methods", "md_doc_2_r_p_c.html#autotoc_md280", [
        [ "taraxa_protocolVersion", "md_doc_2_r_p_c.html#autotoc_md281", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md282", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md283", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md284", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc_2_r_p_c.html#autotoc_md285", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md286", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md287", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md288", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc_2_r_p_c.html#autotoc_md289", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md290", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md291", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md292", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc_2_r_p_c.html#autotoc_md293", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md294", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md295", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md296", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc_2_r_p_c.html#autotoc_md297", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md298", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md299", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md300", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc_2_r_p_c.html#autotoc_md301", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md302", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md303", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md304", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc_2_r_p_c.html#autotoc_md305", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md306", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md307", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md308", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc_2_r_p_c.html#autotoc_md309", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md310", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md311", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md312", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc_2_r_p_c.html#autotoc_md313", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md314", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md315", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md316", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc_2_r_p_c.html#autotoc_md317", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md318", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md319", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md320", null ]
        ] ],
        [ "taraxa_yield", "md_doc_2_r_p_c.html#autotoc_md321", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md322", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md323", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md324", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc_2_r_p_c.html#autotoc_md325", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md326", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md327", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md328", null ]
        ] ],
        [ "taraxa_getPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md329", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md330", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md331", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md332", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc_2_r_p_c.html#autotoc_md333", [
        [ "get_sortition_change", "md_doc_2_r_p_c.html#autotoc_md334", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md335", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md336", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md337", null ]
        ] ],
        [ "send_coin_transaction", "md_doc_2_r_p_c.html#autotoc_md338", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md339", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md340", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md341", null ]
        ] ],
        [ "send_coin_transactions", "md_doc_2_r_p_c.html#autotoc_md342", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md343", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md344", null ]
        ] ],
        [ "get_account_address", "md_doc_2_r_p_c.html#autotoc_md345", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md346", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md347", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md348", null ]
        ] ],
        [ "get_peer_count", "md_doc_2_r_p_c.html#autotoc_md349", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md350", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md351", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md352", null ]
        ] ],
        [ "get_node_status", "md_doc_2_r_p_c.html#autotoc_md353", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md354", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md355", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md356", null ]
        ] ],
        [ "get_all_nodes", "md_doc_2_r_p_c.html#autotoc_md357", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md358", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md359", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md360", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc_2_r_p_c.html#autotoc_md361", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc_2_r_p_c.html#autotoc_md362", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md363", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md364", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md365", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc_2_r_p_c.html#autotoc_md366", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md367", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md368", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md369", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc_2_r_p_c.html#autotoc_md370", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md371", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md372", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md373", null ]
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
"_log_8h.html#aac97ba82d6c884d2ed35e247e4adec05",
"class_modular_server.html#a9c93b3f82a07078dbe88b27166d69eab",
"classdev_1_1_r_l_p_stream.html#a8a1c77999759b4d5d0bd956268c9e27d",
"classdev_1_1p2p_1_1_node_table.html#ab60d471754e8f86ec531726860600461",
"classgraphql_1_1taraxa_1_1_current_state.html",
"classtaraxa_1_1_db_storage.html#a1389c4d576ca86153be5668efdff49de",
"classtaraxa_1_1_network.html#a5c21890f89ed4e454378d7c6392a95e6",
"classtaraxa_1_1net_1_1_debug_face.html#ab3fabc7f4cb811646751de0eeb3b5a1d",
"classtaraxa_1_1net_1_1_subscriptions.html#a6bdaf8f800cb41508932779b112fe1e7",
"classtaraxa_1_1net_1_1rpc_1_1eth_1_1_watch_group.html#abc331f00d30d0e38784dc2d988876777",
"classtaraxa_1_1network_1_1tarcap_1_1_peers_state.html#a2254e736b6cc5ac7d1adc6ed81afae28",
"classtaraxa_1_1pillar__chain_1_1_pillar_votes.html#a097db60f8bab5275de012929ec64b48e",
"cli_2include_2cli_2config_8hpp.html#aeb64675c1abb0e6e331e98130df18ca5",
"dir_f8e737bc640485511cd1d4de3065b187.html",
"group___d_a_g.html#a48dff2f0634ce367582074273414ce44",
"group___final_chain.html#a7c2017fb88bdb3abb1f0ebc82ec1fd83",
"group___p_b_f_t.html#a9f4d1fa2abfe7661fe86659aadca662a",
"group___transaction.html#aa5cbbeba5ba91bd6bfea36083cc7517f",
"interface_2dag__block__packet__handler_8hpp.html",
"libp2p_2_common_8h.html#adabfa58f948d1c67b5d7951e7c4d04e7a2d122190369b9e5bdf4e3f1e58e4ed3b",
"md_doc_2release__cycle.html#autotoc_md238",
"namespacetaraxa_1_1network_1_1tarcap.html#structtaraxa_1_1network_1_1tarcap_1_1_pbft_sync_packet",
"state__config_8cpp.html#a88679bd672ad12f8a2517728f1c1109f",
"structdev_1_1p2p_1_1_host.html#ac23d1da633a7cd9f7e02dcdbf82808af",
"structdev_1_1p2p_1_1_session.html#ac3a2ee5a4288cbf52bc71761ff25c58a",
"structtaraxa_1_1_transaction.html#af2673ba70c888218de98485d5018af0f",
"transaction__receipts__by__period_8cpp.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';