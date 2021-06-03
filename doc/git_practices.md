Git-flow Guide
-------------------

## Branch naming conventions

In Taraxa, we are using git-flow workflow based on this [git-branching-model](https://nvie.com/posts/a-successful-git-branching-model/).

![Git branching model](./images/git_model.png?raw=true "Git branching model")

## Main branches
    master, develop

## Supporting branches
    Feature branches, Hot-fix Branches, Release Branches
Unlike the main branches (master, develop) these branches always have a limited life time since they will be removed eventually.
Each of these branches have a specific purpose, e.g. developing a new feature.

### Standard Feature branches

Feature branches are used to develop new features and merged back to develop. Basically all features, bug-fixes, refactors, etc...

May branch off from:  
`develop`

Must merge back into:  
`develop`

Branch naming convention:  
`no prefix` is used as 99% of branches will be feature branches, such prefix would be redundant.

### Long-term Feature branches

Same logic applies as for `Standard feature branches` with one minor change. This is a feature that we expect to work on for
relatively long time and multiple developers might be working on it simultaneously. They would use this `feature/*` branch as base branch so it would
basically become a temporary develop for them...  

Such cases are rare though...

May branch off from:  
`develop`

Must merge back into:  
`develop`

Branch naming convention:  
`feature/*`


### Hotfix branches

Hotfix branches are very much like release branches in that they are also meant to prepare for a new production release, albeit unplanned.
They arise from the necessity to act immediately upon an undesired state of a live production version.
When a critical bug in a production version must be resolved immediately, a hotfix branch may be branched off from the corresponding tag on the master branch that marks the production version.

May branch off from:  
`master`

Must merge back into:  
`develop` and `master`

Branch naming convention:  
`hotfix/*`


### Release branches

Release branches support the preparation of a new production release. They allow for minor bug fixes (can be contained in bugfix branch) and preparing
meta-data for a release. By doing all of this work on a release branch, the develop branch is cleared to receive features for the next big release.

Next, that commit on master must be tagged for easy future reference to this historical version. Finally, the changes
made on the release branch need to be merged back into develop, so that future releases also contain these bug fixes.

May branch off from:  
`develop`

Must merge back into:  
`develop` and `master`

Branch naming convention:  
`release/*`

## Branches Cleaning
Every month will be deleted by automatic script(TODO):
- feature branches older than 1 months
- unknown branches older than 1 months
- branches fully merged to master/develop


## PR merging & Code reviews
When a developer is finished working on an feature or issue, another developer looks over the code and considers questions like:
- Are there any obvious logic errors in the code?
- Looking at the requirements, are all cases fully implemented?
- Are the new automated tests sufficient for the new code? Do existing automated tests need to be rewritten to account for changes in the code?
- Does the new code conform to existing coding guidelines?

Requiring code review before merging upstream ensures that no code gets in unreviewed. Another required action before enabling merge are successful
builds and all kind of tests on all platform. At least 2 approvals are required before PR can be merged.

    !!! Important:

    // If there were some new commits pushed to the base branch while working on your branch, always use rebase pull
    git pull --rebase origin <BASE BRANCH>>

# Commit message conventions

In Taraxa, we are using commit messages based on this [conventions](http://karma-runner.github.io/1.0/dev/git-commit-msg.html).

A typical git commit message will look like:

    <type>(<scope>): <subject>

### "type" must be one of the following mentioned below!
`build`: Build related changes (eg: npm related/ adding external dependencies)    
`chore`: A code change that external user won't see (won't affect him), e.g.: (change to .gitignore file, change to internal build system - jenkins, merge conflicts fixes, etc..)  
`feat`: A new feature  
`fix`: A bug fix  
`docs`: Documentation related changes  
`refactor`: A code that neither fix bug nor adds a feature. (eg: You can use this when there is semantic changes like renaming a variable/ function name)  
`perf`: A code that improves performance  
`style`: A code that is related to styling  
`test`: Adding new test or making changes to existing test

### "scope" is optional
Scope must be noun and it represents the section of the section of the codebase, e.g. consesnus, dag, network, etc...

### "subject"
use imperative, present tense (eg: use "add" instead of "added" or "adds")  
don't use dot(.) at end  
don't capitalize first letter


# Automatic github issues linking
We use [github issues](https://github.com/Taraxa-project/taraxa-node/issues) for tasks/tracking. To automatically link your PR to the
github issue (and vice versa), use branch naming convention:

`issue-<NUMBER>/*`

e.g. :

    issue-1234/new-best-feature

# Example

Let's take this doc change as an example.

- According to our conventions, it is a `standard feature` branch, so `no prefix` for branch name should be used
- As it is feature branch, it can be branched off only from `develop`
- There is an existing [github issue #760](https://github.com/Taraxa-project/taraxa-node/issues/760) for this feature,
  so `issue-<NUMBER>/*` branch naming convention should be used
- Documentation related changes are being added, so `docs` commit type is used
- Commit scope is optional, but we can use `git-flow` in this example, code-related scopes might be for examples: `consensus`, `dag`, etc...

### Used git flow:

    git checkout develop
    
    git checkout -b "issue-760/proposed-git-flow-conventions"
    // do changes to the repo files...
    git add <files>
    git commit -m "docs(git-flow): added proposed conventions for git-flow (branches, commits)"
    git push origin issue-760/proposed-git-flow-conventions
    
    // If there were some new changes added to the develop branch in the meantime, use rebase pull
    git pull --rebase origin develop

    // create PR back to develop

