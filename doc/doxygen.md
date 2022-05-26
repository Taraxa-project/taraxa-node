# Doxygen

## Installing dependencies 

### Ubuntu 

```bash
apt install doxygen graphviz
```

### MacOS
```bash
brew install doxygen graphviz
```

## Generating documentation

To generate documentation you need to call `doxygen` from project source root. It should generate all needed files in `doxygen_docs` folder. You can open `index.html` in your browser to check result.

## Commenting 

Full description on [doxygen site](https://www.doxygen.nl/manual/docblocks.html)

### Basic example

```cpp
/** @brief Brief description which ends at this dot. 
 *
 * Details follows here.
 * @see anotherMethod()
 * @param param1 description
 * @param params2 description
 * @return return value description
 */
```

### Grouping to modules 

We should also group code to modules, so it will be easier to navigate in documentation. Class with all methods would be added as `Test` module. Example:

```cpp
/** @addtogroup Test
 * @{
 */

class TestClass {}

/**
 * @}
 */
```

### Graphs 

Collaboration diagrams enabled by default for all classes. Additionally we could add `call graph` and `caller graph` for functions and method. To do that you need to specify additional attributes to comment:


```cpp
/**
* @callergraph 
* @callgraph
* @brief brief description of function or method 
....
*/
```
