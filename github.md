

# github and git commands #


  * Configured remote repository for your fork.
`git remote -v
origin  https://github.com/YOUR_USERNAME/YOUR_FORK.git (fetch)
origin  https://github.com/YOUR_USERNAME/YOUR_FORK.git (push)`



  * Configuring a git-remote name **upstream** for a fork repository on github
 ` git remote add upstream https://github.com/ORIGINAL_OWNER/ORIGINAL_REPOSITORY.git`
 
 
  
  * Sync the repository with a forked repository ; the commits to REMOTE **master** are in a local branch, **upstream/master.**
 ` git fetch upstream`
 
  *  Do the checkout to the local master
` git checkout master` 


  * Merge the changes from upstream/master into your local master branc; if there is no modifications a fast-forward is done.
` git merge upstream/master` 
