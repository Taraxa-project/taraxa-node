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
    [ "Introducing Taraxa", "index.html", [
      [ "Whitepaper", "index.html#autotoc_md1", null ],
      [ "Quickstart", "index.html#autotoc_md2", null ],
      [ "Downloading", "index.html#autotoc_md3", null ],
      [ "Building", "index.html#autotoc_md6", null ],
      [ "Running", "index.html#autotoc_md7", null ],
      [ "Contributing", "index.html#autotoc_md10", null ],
      [ "Useful doc", "index.html#autotoc_md11", null ],
      [ "System Requirements", "index.html#autotoc_md12", null ]
    ] ],
    [ "Multiple taraxa capabilities support", "md_libraries_core_libs_network_src_tarcap_packets_handlers_latest_readme.html", null ],
    [ "Building taraxa-node", "md_doc_building.html", [
      [ "Building on Ubuntu 22.04", "md_doc_building.html#autotoc_md30", [
        [ "Compile-Time Options (cmake)", "md_doc_building.html#autotoc_md28", [
          [ "Docker image", "index.html#autotoc_md4", null ],
          [ "Ubuntu binary", "index.html#autotoc_md5", null ],
          [ "Inside docker image", "index.html#autotoc_md8", null ],
          [ "Pre-built binary or manual build:", "index.html#autotoc_md9", null ],
          [ "CMAKE_BUILD_TYPE=[Release/Debug/RelWithDebInfo]", "md_doc_building.html#autotoc_md29", null ]
        ] ],
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md31", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md32", null ],
        [ "Compile", "md_doc_building.html#autotoc_md33", null ]
      ] ],
      [ "Building on Ubuntu 20.04", "md_doc_building.html#autotoc_md34", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md35", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md36", null ],
        [ "Compile", "md_doc_building.html#autotoc_md37", null ]
      ] ],
      [ "Building on MacOS", "md_doc_building.html#autotoc_md38", [
        [ "Install taraxa-node dependencies:", "md_doc_building.html#autotoc_md39", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md40", null ],
        [ "Compile", "md_doc_building.html#autotoc_md41", null ],
        [ "Known issues", "md_doc_building.html#autotoc_md42", [
          [ "Issues with conan cache", "md_doc_building.html#autotoc_md43", null ]
        ] ]
      ] ],
      [ "Building on M1 Macs for x86_64 with Rosetta2", "md_doc_building.html#autotoc_md44", [
        [ "Install Rosetta2", "md_doc_building.html#autotoc_md45", null ],
        [ "Run an x86_64 session", "md_doc_building.html#autotoc_md46", null ],
        [ "Install Homebrew", "md_doc_building.html#autotoc_md47", null ],
        [ "Install dependencies", "md_doc_building.html#autotoc_md48", null ],
        [ "Clone the Repository", "md_doc_building.html#autotoc_md49", null ],
        [ "Compile", "md_doc_building.html#autotoc_md50", null ]
      ] ],
      [ "Run", "md_doc_building.html#autotoc_md51", [
        [ "Running tests", "md_doc_building.html#autotoc_md52", null ],
        [ "Running taraxa-node", "md_doc_building.html#autotoc_md53", null ]
      ] ],
      [ "Building using \"taraxa-builder\" docker image", "md_doc_building.html#autotoc_md54", null ]
    ] ],
    [ "C++ Best Practices Guidelines", "md_doc_coding_practices.html", [
      [ "The structure of this document", "md_doc_coding_practices.html#autotoc_md56", [
        [ "<a name=\"compiletimechecking\"></a> 1. Prefer compile-time checking to run-time checking", "md_doc_coding_practices.html#autotoc_md57", null ],
        [ "<a name=\"avoidnonconst\"></a> 2. Avoid non-<tt>const</tt> global variables", "md_doc_coding_practices.html#autotoc_md61", null ],
        [ "<a name=\"ruleofzero\"></a> 3. If you can avoid defining default operations, do", "md_doc_coding_practices.html#autotoc_md68", null ],
        [ "<a name=\"ruleoffive\"></a> 4. If you define or <tt>=delete</tt> any default operation, define or <tt>=delete</tt> them all", "md_doc_coding_practices.html#autotoc_md73", null ],
        [ "<a name=\"smartptr\"></a> 5. Never transfer ownership by a raw pointer (<tt>T*</tt>) or reference (<tt>T&</tt>)", "md_doc_coding_practices.html#autotoc_md82", null ],
        [ "<a name=\"usingsmartptr\"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?", "md_doc_coding_practices.html#autotoc_md87", null ],
        [ "<a name=\"singlealloc\"></a> 7. Perform at most one explicit resource allocation in a single expression statement", "md_doc_coding_practices.html#autotoc_md91", null ],
        [ "<a name=\"passsmartptr\"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics", "md_doc_coding_practices.html#autotoc_md95", null ],
        [ "<a name=\"varlimitscope\"></a> 9. Declare names in for-statement initializers and conditions to limit scope", "md_doc_coding_practices.html#autotoc_md100", null ],
        [ "<a name=\"varrecycle\"></a> 10. Don't use a variable for two unrelated purposes", "md_doc_coding_practices.html#autotoc_md106", null ],
        [ "<a name=\"macrosconsts\"></a> 11. Don't use macros for constants or \"functions\"", "md_doc_coding_practices.html#autotoc_md111", null ],
        [ "<a name=\"magicconstants\"></a> 12. Avoid \"magic constants\"; use symbolic constants", "md_doc_coding_practices.html#autotoc_md115", null ],
        [ "<a name=\"nullptr\"></a> 13. Use <tt>nullptr</tt> rather than <tt>0</tt> or <tt>NULL</tt>", "md_doc_coding_practices.html#autotoc_md119", null ],
        [ "<a name=\"construct\"></a> 14. Use the <tt>T{e}</tt>notation for construction", "md_doc_coding_practices.html#autotoc_md123", null ],
        [ "<a name=\"referencecapture\"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms", "md_doc_coding_practices.html#autotoc_md129", null ],
        [ "<a name=\"valuecapture\"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread", "md_doc_coding_practices.html#autotoc_md136", null ],
        [ "<a name=\"thiscapture\"></a> 17. If you capture <tt>this</tt>, capture all variables explicitly (no default capture)", "md_doc_coding_practices.html#autotoc_md141", null ],
        [ "<a name=\"defaultctor\"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead", "md_doc_coding_practices.html#autotoc_md146", null ],
        [ "<a name=\"explicitctor\"></a> 19. By default, declare single-argument constructors explicit", "md_doc_coding_practices.html#autotoc_md151", null ],
        [ "<a name=\"alwaysinitialize\"></a> 20. Always initialize an object", "md_doc_coding_practices.html#autotoc_md156", null ],
        [ "<a name=\"lambdainit\"></a> 21. Use lambdas for complex initialization, especially of <tt>const</tt> variables", "md_doc_coding_practices.html#autotoc_md167", null ],
        [ "<a name=\"orderctor\"></a> 22. Define and initialize member variables in the order of member declaration", "md_doc_coding_practices.html#autotoc_md173", null ],
        [ "<a name=\"inclassinitializer\"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers", "md_doc_coding_practices.html#autotoc_md177", null ],
        [ "<a name=\"initializetoassignment\"></a> 24. Prefer initialization to assignment in constructors", "md_doc_coding_practices.html#autotoc_md182", null ],
        [ "<a name=\"castsnamed\"></a> 25. If you must use a cast, use a named cast", "md_doc_coding_practices.html#autotoc_md186", null ],
        [ "<a name=\"castsconst\"></a> 26. Don't cast away <tt>const</tt>", "md_doc_coding_practices.html#autotoc_md192", null ],
        [ "<a name=\"symmetric\"></a> 27. Use nonmember functions for symmetric operators", "md_doc_coding_practices.html#autotoc_md199", null ]
      ] ]
    ] ],
    [ "Contributing Guide", "md_doc_contributing.html", null ],
    [ "Doxygen", "md_doc_doxygen.html", [
      [ "Installing dependencies", "md_doc_doxygen.html#autotoc_md205", [
        [ "Ubuntu", "md_doc_doxygen.html#autotoc_md206", null ],
        [ "MacOS", "md_doc_doxygen.html#autotoc_md207", null ]
      ] ],
      [ "Generating documentation", "md_doc_doxygen.html#autotoc_md208", null ],
      [ "Commenting", "md_doc_doxygen.html#autotoc_md209", [
        [ "Basic example", "md_doc_doxygen.html#autotoc_md210", null ],
        [ "Grouping to modules", "md_doc_doxygen.html#autotoc_md211", null ],
        [ "Graphs", "md_doc_doxygen.html#autotoc_md212", null ]
      ] ]
    ] ],
    [ "Git-flow Guide", "md_doc_git_practices.html", [
      [ "Branch naming conventions", "md_doc_git_practices.html#autotoc_md213", null ],
      [ "Main branches", "md_doc_git_practices.html#autotoc_md214", null ],
      [ "Supporting branches", "md_doc_git_practices.html#autotoc_md215", [
        [ "Standard Feature branches", "md_doc_git_practices.html#autotoc_md216", null ],
        [ "Long-term Feature branches", "md_doc_git_practices.html#autotoc_md217", null ],
        [ "Hotfix branches", "md_doc_git_practices.html#autotoc_md218", null ],
        [ "Release branches", "md_doc_git_practices.html#autotoc_md219", null ]
      ] ],
      [ "Branches Cleaning", "md_doc_git_practices.html#autotoc_md220", null ],
      [ "PR merging & Code reviews", "md_doc_git_practices.html#autotoc_md221", null ],
      [ "Commit message conventions", "md_doc_git_practices.html#autotoc_md222", null ],
      [ "Automatic github issues linking", "md_doc_git_practices.html#autotoc_md226", null ],
      [ "Example", "md_doc_git_practices.html#autotoc_md227", null ]
    ] ],
    [ "Quickstart Guide", "md_doc_quickstart_guide.html", [
      [ "Taraxa docker image", "md_doc_quickstart_guide.html#autotoc_md236", [
        [ "Pre-requisites", "md_doc_quickstart_guide.html#autotoc_md229", [
          [ "\"type\" must be one of the following mentioned below!", "md_doc_git_practices.html#autotoc_md223", null ],
          [ "\"scope\" is optional", "md_doc_git_practices.html#autotoc_md224", null ],
          [ "\"subject\"", "md_doc_git_practices.html#autotoc_md225", null ],
          [ "Used git flow:", "md_doc_git_practices.html#autotoc_md228", null ],
          [ "MANDATORY PORT", "md_doc_quickstart_guide.html#autotoc_md230", null ],
          [ "OPTIONAL PORTS", "md_doc_quickstart_guide.html#autotoc_md231", null ]
        ] ],
        [ "Config", "md_doc_quickstart_guide.html#autotoc_md232", [
          [ "Param1", "md_doc_quickstart_guide.html#autotoc_md233", null ],
          [ "Param2", "md_doc_quickstart_guide.html#autotoc_md234", null ],
          [ "...", "md_doc_quickstart_guide.html#autotoc_md235", null ],
          [ "taraxa-builder:latest", "md_doc_quickstart_guide.html#autotoc_md237", null ],
          [ "taraxa-node:latest", "md_doc_quickstart_guide.html#autotoc_md238", null ]
        ] ]
      ] ]
    ] ],
    [ "Standard release cycle", "md_doc_release_cycle.html", [
      [ "Release cycle phases", "md_doc_release_cycle.html#autotoc_md240", [
        [ "Phase 1 - active development of new features", "md_doc_release_cycle.html#autotoc_md241", null ],
        [ "Phase 2 - alpha testing (internal)", "md_doc_release_cycle.html#autotoc_md242", null ],
        [ "Phase 3 - beta testing (public)", "md_doc_release_cycle.html#autotoc_md243", null ],
        [ "Phase 4 - Mainnet release", "md_doc_release_cycle.html#autotoc_md244", null ]
      ] ],
      [ "Ad-hoc releases with bug fixes", "md_doc_release_cycle.html#autotoc_md245", null ]
    ] ],
    [ "Rewards distribution algorithm", "md_doc_rewards_distribution.html", [
      [ "Glossary", "md_doc_rewards_distribution.html#autotoc_md247", null ],
      [ "Rewards sources", "md_doc_rewards_distribution.html#autotoc_md248", null ],
      [ "Rewards distribution", "md_doc_rewards_distribution.html#autotoc_md249", [
        [ "Beneficial work in network", "md_doc_rewards_distribution.html#autotoc_md250", null ],
        [ "Newly created tokens:", "md_doc_rewards_distribution.html#autotoc_md251", null ],
        [ "Included transactions fees:", "md_doc_rewards_distribution.html#autotoc_md252", null ]
      ] ],
      [ "Validators statistics", "md_doc_rewards_distribution.html#autotoc_md253", null ],
      [ "Example:", "md_doc_rewards_distribution.html#autotoc_md254", [
        [ "DAG structure:", "md_doc_rewards_distribution.html#autotoc_md255", null ],
        [ "PBFT block", "md_doc_rewards_distribution.html#autotoc_md256", null ],
        [ "Statistics", "md_doc_rewards_distribution.html#autotoc_md257", null ],
        [ "Rewards", "md_doc_rewards_distribution.html#autotoc_md258", [
          [ "DAG blocks rewards", "md_doc_rewards_distribution.html#autotoc_md259", null ],
          [ "PBFT proposer reward", "md_doc_rewards_distribution.html#autotoc_md260", null ],
          [ "PBFT voters reward", "md_doc_rewards_distribution.html#autotoc_md261", null ]
        ] ]
      ] ]
    ] ],
    [ "Taraxa RPC", "md_doc__r_p_c.html", [
      [ "Ethereum compatibility", "md_doc__r_p_c.html#autotoc_md263", [
        [ "Quirks", "md_doc__r_p_c.html#autotoc_md264", null ],
        [ "Not implemented", "md_doc__r_p_c.html#autotoc_md265", null ]
      ] ],
      [ "Taraxa specific methods", "md_doc__r_p_c.html#autotoc_md266", [
        [ "taraxa_protocolVersion", "md_doc__r_p_c.html#autotoc_md267", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md268", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md269", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md270", null ]
        ] ],
        [ "taraxa_getVersion", "md_doc__r_p_c.html#autotoc_md271", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md272", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md273", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md274", null ]
        ] ],
        [ "taraxa_getDagBlockByHash", "md_doc__r_p_c.html#autotoc_md275", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md276", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md277", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md278", null ]
        ] ],
        [ "taraxa_getDagBlockByLevel", "md_doc__r_p_c.html#autotoc_md279", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md280", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md281", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md282", null ]
        ] ],
        [ "taraxa_dagBlockLevel", "md_doc__r_p_c.html#autotoc_md283", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md284", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md285", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md286", null ]
        ] ],
        [ "taraxa_dagBlockPeriod", "md_doc__r_p_c.html#autotoc_md287", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md288", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md289", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md290", null ]
        ] ],
        [ "taraxa_getScheduleBlockByPeriod", "md_doc__r_p_c.html#autotoc_md291", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md292", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md293", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md294", null ]
        ] ],
        [ "taraxa_pbftBlockHashByPeriod", "md_doc__r_p_c.html#autotoc_md295", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md296", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md297", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md298", null ]
        ] ],
        [ "taraxa_getConfig", "md_doc__r_p_c.html#autotoc_md299", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md300", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md301", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md302", null ]
        ] ],
        [ "taraxa_getChainStats", "md_doc__r_p_c.html#autotoc_md303", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md304", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md305", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md306", null ]
        ] ],
        [ "taraxa_yield", "md_doc__r_p_c.html#autotoc_md307", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md308", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md309", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md310", null ]
        ] ],
        [ "taraxa_totalSupply", "md_doc__r_p_c.html#autotoc_md311", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md312", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md313", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md314", null ]
        ] ]
      ] ],
      [ "Test API", "md_doc__r_p_c.html#autotoc_md315", [
        [ "get_sortition_change", "md_doc__r_p_c.html#autotoc_md316", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md317", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md318", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md319", null ]
        ] ],
        [ "send_coin_transaction", "md_doc__r_p_c.html#autotoc_md320", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md321", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md322", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md323", null ]
        ] ],
        [ "send_coin_transactions", "md_doc__r_p_c.html#autotoc_md324", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md325", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md326", null ]
        ] ],
        [ "get_account_address", "md_doc__r_p_c.html#autotoc_md327", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md328", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md329", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md330", null ]
        ] ],
        [ "get_peer_count", "md_doc__r_p_c.html#autotoc_md331", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md332", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md333", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md334", null ]
        ] ],
        [ "get_node_status", "md_doc__r_p_c.html#autotoc_md335", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md336", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md337", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md338", null ]
        ] ],
        [ "get_all_nodes", "md_doc__r_p_c.html#autotoc_md339", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md340", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md341", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md342", null ]
        ] ]
      ] ],
      [ "Debug API", "md_doc__r_p_c.html#autotoc_md343", [
        [ "debug_getPeriodTransactionsWithReceipts", "md_doc__r_p_c.html#autotoc_md344", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md345", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md346", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md347", null ]
        ] ],
        [ "debug_getPeriodDagBlocks", "md_doc__r_p_c.html#autotoc_md348", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md349", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md350", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md351", null ]
        ] ],
        [ "debug_getPreviousBlockCertVotes", "md_doc__r_p_c.html#autotoc_md352", [
          [ "Parameters", "md_doc__r_p_c.html#autotoc_md353", null ],
          [ "Returns", "md_doc__r_p_c.html#autotoc_md354", null ],
          [ "Example", "md_doc__r_p_c.html#autotoc_md355", null ]
        ] ]
      ] ]
    ] ],
    [ "readme", "md_doc_uml_readme.html", null ],
    [ "Todo List", "todo.html", null ],
    [ "Modules", "modules.html", "modules" ],
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
        [ "Related Functions", "functions_rela.html", null ]
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
"_log_8h.html#a9e9559eae7bb66811be1df68edf2e877",
"class_modular_server_3_01_i_00_01_is_8_8_8_01_4.html#ac103c344d481c55a0ec4fa5f228029ff",
"classdev_1_1_r_l_p_stream.html#ac8eb3fe628f61f13d7fc02d0633ad53a",
"classdev_1_1p2p_1_1_node_table.html#ae1d9a153f6d72f9fd7915952d55ec69f",
"classgraphql_1_1taraxa_1_1_dag_block.html#a4887648782bbf8804a839821c0136683",
"classtaraxa_1_1_db_storage.html#a902f33070cce3963e874a36dd64b4a8a",
"classtaraxa_1_1_status_table.html#a60c5a4f1d1b3a10dc474fc7509ef7c01",
"classtaraxa_1_1net_1_1_debug_face.html#aea4208ecbed321daa2ce87b38eb9b9c1",
"classtaraxa_1_1net_1_1_taraxa_face.html#a57daf15be3c242029cd51fa7a33d6f33",
"classtaraxa_1_1network_1_1tarcap_1_1_ext_votes_packet_handler.html#ab389ecf32012656604e2b80e1620438b",
"classtaraxa_1_1network_1_1tarcap_1_1_transaction_packet_handler.html#a3e0874d6cfd20eeed43d18249262ff18",
"classtaraxa_1_1vdf__sortition_1_1_vdf_sortition.html#aa653d6c4ce76eb42bc0878c57ddd726c",
"dir_f8e737bc640485511cd1d4de3065b187.html",
"group___d_a_g.html#a4ab4fb411fe6447b8a556a53c1ea700c",
"group___final_chain.html#ab97f01ac653897edd3b161552b62802d",
"group___p_b_f_t.html#af75c5530bf8d473c9213611ea6a3b9c4",
"hardfork_8cpp.html#a280d6fe653a1c7e6c87fd8f2162a2090",
"libp2p_2_common_8h.html#a6604731ce0c5ace0e4ee202b55acb40c",
"md_doc_coding_practices.html#autotoc_md155",
"namespacetaraxa.html#a3747de5ec38a001ef934c2764b539ef3",
"state__config_8cpp.html#a71678cb3f5feb67a738df3d7d2f45270",
"structdev_1_1p2p_1_1_host.html#adf3f35da26402aa18fff85051f373267",
"structdev_1_1p2p_1_1_session.html#afa5796875b94e6dbb2601c4922874edb",
"structtaraxa_1_1net_1_1rpc_1_1eth_1_1_log_filter.html#af80eca295cd68501c2f075783e02787d",
"vdf_2src_2config_8cpp.html#a1a5366250b143fb8f68102999cd2f746"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';