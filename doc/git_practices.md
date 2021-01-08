Git-flow Guide
-------------------

In Taraxa, we are using git-flow workflow based on this [git-branching-model](https://nvie.com/posts/a-successful-git-branching-model/).

![Git branching model](./images/git_model.png?raw=true "Git branching model")

### Main branches
    master, develop


### Supporting branches
    Feature branches, Hot-fix Branches, Release Branches
Unlike the main branches (master, develop) these branches always have a limited life time since they will be removed eventually.
Each of these branches have a specific purpose, e.g. developing a new feature.

#### Feature branches

Feature branches are used to develop new features and merged back to develop. 

#### Hotfix branches

Hotfix branches are very much like release branches in that they are
also meant to prepare for a new production release, albeit unplanned. They arise from the necessity to act immediately upon an undesired state of a live
production version.

#### Release branches

Release branches support the preparation of a new production release. They allow for minor bug fixes (can be contained in bugfix branch) and preparing
meta-data for a release. By doing all of this work on a release branch, the develop branch is cleared to receive features for the next big release.

Next, that commit on master must be tagged for easy future reference to this historical version. Finally, the changes
made on the release branch need to be merged back into develop, so that future releases also contain these bug fixes.


### Cleaning
Every month will be deleted by automatic script(TODO):
- feature branches older than 1 months
- unknown branches older than 1 months
- branches fully merged to master/develop


### PR merging & Code reviews
When a developer is finished working on an feature or issue, another developer looks over the code and considers questions like:
- Are there any obvious logic errors in the code?
- Looking at the requirements, are all cases fully implemented?
- Are the new automated tests sufficient for the new code? Do existing automated tests need to be rewritten to account for changes in the code?
- Does the new code conform to existing coding guidelines?

Requiring code review before merging upstream ensures that no code gets in unreviewed. Another required action before enabling merge are successful
builds and all kind of tests on all platform. At least 2 approvals are required before PR can be merged.