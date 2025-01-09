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
      [ "go-ethereum library", "md_doc_2evm__incompatibilities.html#autotoc_md212", null ]
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
        [ "Not implemented", "md_doc_2_r_p_c.html#autotoc_md265", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc_2_r_p_c.html#autotoc_md266", [
        [ "taraxa_protocolVersion", "md_doc_2_r_p_c.html#autotoc_md267", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md268", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md269", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md270", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc_2_r_p_c.html#autotoc_md271", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md272", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md273", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md274", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc_2_r_p_c.html#autotoc_md275", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md276", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md277", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md278", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc_2_r_p_c.html#autotoc_md279", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md280", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md281", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md282", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc_2_r_p_c.html#autotoc_md283", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md284", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md285", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md286", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc_2_r_p_c.html#autotoc_md287", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md288", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md289", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md290", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc_2_r_p_c.html#autotoc_md291", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md292", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md293", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md294", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc_2_r_p_c.html#autotoc_md295", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md296", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md297", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md298", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc_2_r_p_c.html#autotoc_md299", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md300", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md301", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md302", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc_2_r_p_c.html#autotoc_md303", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md304", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md305", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md306", null ]
        ] ],
        [ "taraxa_yield", "md_doc_2_r_p_c.html#autotoc_md307", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md308", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md309", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md310", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc_2_r_p_c.html#autotoc_md311", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md312", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md313", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md314", null ]
        ] ],
        [ "taraxa_getPillarBlockData", "md_doc_2_r_p_c.html#autotoc_md315", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md316", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md317", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md318", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc_2_r_p_c.html#autotoc_md319", [
        [ "get_sortition_change", "md_doc_2_r_p_c.html#autotoc_md320", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md321", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md322", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md323", null ]
        ] ],
        [ "send_coin_transaction", "md_doc_2_r_p_c.html#autotoc_md324", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md325", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md326", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md327", null ]
        ] ],
        [ "send_coin_transactions", "md_doc_2_r_p_c.html#autotoc_md328", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md329", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md330", null ]
        ] ],
        [ "get_account_address", "md_doc_2_r_p_c.html#autotoc_md331", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md332", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md333", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md334", null ]
        ] ],
        [ "get_peer_count", "md_doc_2_r_p_c.html#autotoc_md335", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md336", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md337", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md338", null ]
        ] ],
        [ "get_node_status", "md_doc_2_r_p_c.html#autotoc_md339", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md340", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md341", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md342", null ]
        ] ],
        [ "get_all_nodes", "md_doc_2_r_p_c.html#autotoc_md343", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md344", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md345", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md346", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc_2_r_p_c.html#autotoc_md347", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc_2_r_p_c.html#autotoc_md348", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md349", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md350", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md351", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc_2_r_p_c.html#autotoc_md352", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md353", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md354", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md355", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc_2_r_p_c.html#autotoc_md356", [
          [ "Parameters", "md_doc_2_r_p_c.html#autotoc_md357", null ],
          [ "Returns", "md_doc_2_r_p_c.html#autotoc_md358", null ],
          [ "Example", "md_doc_2_r_p_c.html#autotoc_md359", null ]
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
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_a_e_s_8cpp.html",
"_log_8h.html#aba52eae6cb2490d05b7e7b93b105d93e",
"class_modular_server_3_01_i_00_01_is_8_8_8_01_4.html#ac103c344d481c55a0ec4fa5f228029ff",
"classdev_1_1_r_l_p_stream.html#ac00a7283d9dd08443e166d9443bcef02",
"classdev_1_1p2p_1_1_node_table.html#addda5e37633fabc8e2a4038b9ebfa28a",
"classgraphql_1_1taraxa_1_1_dag_block.html#a1f6138a2169e7ec5f5a03462eef34094",
"classtaraxa_1_1_db_storage.html#a88a606470708384489145d70b5e1c11b",
"classtaraxa_1_1_proposed_blocks.html#a83ac95e2efe8f0859b3fcf1d0deb450e",
"classtaraxa_1_1net_1_1_eth_face.html#a093d42538582eac75cf09aec275e5c24",
"classtaraxa_1_1net_1_1_test_client.html#a347a4180845878faed3984decfbb0445",
"classtaraxa_1_1network_1_1tarcap_1_1_get_dag_sync_packet_handler.html#acbf5e3be4a8bff032f757845621f47d8",
"classtaraxa_1_1network_1_1tarcap_1_1_time_period_packets_stats.html#afa45dffa2aac24e5dccb93f7821c23e2",
"classtaraxa_1_1network_1_1threadpool_1_1_packets_queue.html#aa84b418443d6f182e98807baaf236a7e",
"common_prefix-example.html",
"encoding__rlp_8hpp.html#a99f42e0a0c28264c593aa672d27a75a9",
"group___d_a_g.html#a7c5c2559e31fc397527a42cbdfa20128",
"group___final_chain.html#a954304ae0af99b51d175d9ba4a09a976",
"group___p_b_f_t.html#ac0398594ecfd62866c4e88c255284e0f",
"group___transaction.html#ggae7551c287ae57b46d56d27de67df61e7a157d034f9c98a305eb73776582550027",
"latest_2dag__block__packet__handler_8hpp.html",
"logger_8hpp.html#a65ec35addf583c82df610354d05d8769",
"mutation_8hpp.html",
"network_2rpc_2eth_2data_8hpp.html#a340bc91663d886575b83dabfdeda98fc",
"structdev_1_1_converter_3_01std_1_1unordered__set_3_01_t_01_4_01_4.html#ac98ec671ea509dd59e607244314f35e8",
"structdev_1_1p2p_1_1_node_info.html#a9121f24ea6f963cfae1d7c32a7a8ab11",
"structtaraxa_1_1_ficus_hardfork_config.html#afeb460f9f06b8c1da75d993ffc3e03e8",
"structtaraxa_1_1util_1_1_invalid_encoding_size.html",
"vdf_2include_2vdf_2config_8hpp.html#a124545b9fa417446dd410b9c69701744"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';