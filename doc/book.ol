TOC
	BK overview
	Creating repositories
	Working within a repository
	Resyncing changes between repositories
	Resolving conflicts
	Managing repositories
	Tutorial examples
	Command summary
BK Overview
	Basic model
		repository of revision files
		Simple project
			shared master repository
			private per user working repositories
	Working with others
		each user gets a repository
		work happens locally
			lock locally, check in locally
		use resync to move work between repositories
	SCCS model
		talk about what clean does
		talk about make knowing about SCCS
Creating repositories
	Who should do this?
	Starting from scratch
	Importing existing files
		SCCS
		RCS
		text
		tarballs
	Advanced issues
		renames
		branches
Working within your repository
	Get the repository
	editing files
	checking in files
	committing files
ChangeSets
	What is a ChangeSet?
		captures what has changed
		captures the state of the repository as of that change
		allows you to reproduce the repository as of the change
			resync -r..<rev> from to
	When do you create a changeset?
		commit creates them
		must do this before others can see your work
Resyncing changes between repositories
	resyncing
	update/publish
Resolving conflicts
	walk through
Managing repositories
	Basic model
		1 shared repository
		N unshared
		work moves from shared to private
		changes are created in private
		published back to shared
	Advanced model
		multi level
		each group gets its shared repository
Tutorial examples
	Creating a repository
	Check in / check out files
	Commit changes
	resync
Command summary
