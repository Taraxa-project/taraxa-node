# Standard release cycle
For now we decided to go with 6 week release cycle. At the beginning of each release cycle, devs will be assigned tasks that we want to implement in the upcoming release.

***Note:*** 6 week cycle is just default time frame we choose but we might not strictly follow it.

Versioning is based on [Semantic Versioning 2.0.0](https://semver.org/). \

Release branch annotation: `release/vX.Y.Z` \
Release tag annotation: `vX.Y.Z`

![Release cycle](./images/release_cycle.png?raw=true "Release cycle")

## Release cycle phases
We can divide each release cycle into the multiple phases

### Phase 1 - active development of new features
During this phase devs are implementing new features, devnet is regularly restarted (or reset) with each new feature merged.
Devnet might be unstable during this period of time, although devs should try to fix the bugs asap.
In case other teams need something more stable, they can use prnet.

`HowTo:`
- Create new custom branch
- Implement the feature
- Merge it back to develop
- Deploy new devnet

`Duration:` 4 weeks \
`Active Branch:` develop \
`Environment:` devnet

### Phase 2 - alpha testing (internal)
It is used for internal testing. Stress tests are performed on regular basis and the goal is to make the new set of features stable - it is #1 priority to make sure devnet
does not crash and works without any problems. In case everything is fine, devs can continue with development of new features for the next release and merge
them into the develop branch.

At the beginning of alpha testing, `release/vX.Y.Z` branch is created based on `develop` - this is new release candidate and
from now on only bug fixes are merged into `release/vX.Y.Z`. \
Note: this does not affect develop branch, new features can be still merged into the develop branch for the next release.

***!!! Note 1:*** devnet is freezed and deployed strictly only from `release/vX.Y.Z` branch during alpha testing. \
***Note 2:*** alpha testing for new release starts only after previous release cycle was successfully finished.

`HowTo:`
- At the beginning of alpha testing `release/vX.Y.Z` branch is created based on `develop` - this is new release candidate
- Create first commit with adjusted version in `release/vX.Y.Z` branch
- Deploy `release/vX.Y.Z` on devnet
- From now on only bug fixes are merged into `release/vX.Y.Z`. With each fix, devnet is redeployed.
- ***Optional:*** At the end of alpha testing, merge `release/vX.Y.Z` back into `develop` in case there are some important bug fixes

`Duration:` 1 week \
`Active Branch:` release/vX.Y.Z \
`Environment:` devnet

### Phase 3 - beta testing (public)
After the release candidate successfully passed alpha testing, it is deployed on testnet for beta testing.
The goal is to make new release candidate even more stable and production (mainnet) ready by testing it together with community nodes.

`HowTo:`
- We simply continue with already existing `release/vX.Y.Z` branch
- Create commit in case some testnet specific data must be adjusted, e.g. specifying hardfork block, etc...
- Deploy `release/vX.Y.Z` on testnet
- Only bug fixes are merged into `release/vX.Y.Z`. With each fix, testnet is redeployed.
- ***Optional:*** At the end of beta testing, merge `release/vX.Y.Z` back into `develop` in case there are some important bug fixes

`Duration:` 1 week \
`Active Branch:` release/vX.Y.Z \
`Environment:` testnet

### Phase 4 - Mainnet release
After beta testing is successfully finished, we can create final mainnet release.

`HowTo:`
- Again, we continue with already existing `release/vX.Y.Z` branch
- Create commit in case some mainnet specific data must be adjusted, e.g. specifying hardfork block, etc...
- After all changes were made, merge `release/vX.Y.Z` branch into the `develop` as well as `master` branch
- Create new release in github with `vX.Y.Z` tag from latest `master`
- Deploy `vX.Y.Z` tag on mainnet as well as testnet
- Delete `release/vX.Y.Z` branch

`Duration:` 1 day - Monday, beginning of the next release cycle (fifth week of the first cycle) \
`Active Branch:` release/vX.Y.Z -> merged into master & develop \
`Environment:` mainnet

# Ad-hoc releases with bug fixes
There can be also ad-hoc releases with bug fixes for already released versions.
Such releases do not follow standard release process described above   