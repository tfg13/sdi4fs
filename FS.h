/*
 * File:   FS.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 14, 2015, 5:03 PM
 */

#ifndef SDI4FS_FS_H
#define	SDI4FS_FS_H

#include <iostream>
#include <memory>
#include <unordered_map>

#include "DataBlock.h"
#include "DataBlockList.h"
#include "Directory.h"
#include "File.h"
#include "IDataBlockListCreator.h"
#include "IDirectoryEntryListCreator.h"
#include "INode.h"
#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * Core class for the reference implementation of the SDI4FS file system.
 * For full fs specs, see "sdi4fs_spec".
 *
 * Usage:
 * - Mount the filesystem by calling the constructor.
 * - Call the other methods to use the fs, the names should be self-explanatory.
 * - Call umount() to finish.
 *
 * File system consistency is guaranteed under the following circumstances (logic AND):
 *
 * - Methods are only called from one thread.
 * - After using the filesystem umount() is called
 * - Nothing else is called after umount()
 * - No crashes
 *
 * *Not* calling umount() can lead to file inconsistencys (data loss) in modified, but unclosed/unflushed files.
 * This data loss is limited to the modified files, the rest of the fs remains intact even if umount() is missed.
 * Additionally, not calling umount() requires a block map reconstruction on the next mount,
 * which reads through the whole fs several times, and therefore can take a while.
 */
class FS {
public:
    /**
     * Creates (mounts) the filesystem.
     * @param dev device
     */
    FS(STREAM &dev);

    /**
     * Unmounts the filesystem.
     * Caller must close stream given to constructor after this.
     * DO NOT CALL ANYTHING afterwards!
     */
    void umount();

    /**
     * Creates a new directory with the given path.
     * @param absolutePath absolute (!) path, including new directory
     * @return true, iff successful
     */
    bool mkdir(std::string absolutePath);

    /**
     * Removes the directory with the given path.
     * The directory must be empty.
     * @param absolutePath absolute (!) path, including directory to remove
     * @return true, iff successful
     */
    bool rmdir(std::string absolutePath);

    /**
     * Renames (moves) a hardlink.
     * @param sourcePath absolute, old path
     * @param destPath absolute, new path
     * @return true, iff successful
     */
    bool rename(std::string sourcePath, std::string destPath);

    /**
     * Creates an empty (regular) file.
     * @param absolutePath absolute path, including new file
     * @return true, iff successful
     */
    bool touch(std::string absolutePath);

    /**
     * Lists the content of a directory.
     * The listing has the following format:
     * TYPE LINK_COUNTER LOGIC_SIZE DISK_SIZE CREATE_TIME MODIFIY_TIME NAME
     * - TYPE is the INode type, either d or f for directory or file
     * - LINK_COUNTER is the number of hardlinks pointing to this entry
     * - LOGIC_SIZE is the file size in bytes, always zero for directories
     * - DISK_SIZE size in bytes this entry occupies on disk, includes fs metadata, raw content, padding. result is always a multiple of 4K (block size)
     * - CREATE_TIME unix timestamp (uint32_t), creation time of this entry
     * - MODIFY_TIME unix timestamp (uint32_t), last modification time of this entry
     * - NAME hardlink name
     * The list includes all dotfiles.
     * The list includes a header with the column names, iff at least one entry follows.
     * @param absolutePath the absolute path to the directory
     * @param result the listing, one line per file
     * @return true, iff successful
     */
    bool ls(std::string absolutePath, std::list<std::string> &result);

    /**
     * Removes a hardlink to a file.
     * If the file has zero hardlinks pointing to it after this removal,
     * it will be deleted as well.
     * @param absolutePath absolute path to file
     * @return true, iff successful
     */
    bool rm(std::string absolutePath);

    /**
     * Creates a new hardlink to a file.
     * @param sourcePath absolute Path of the new hardlink
     * @param targetPath absolute Path of the existing file
     * @return true, iff successful
     */
    bool link(std::string sourcePath, std::string targetPath);

    /**
     * Convenience function, returns the size (bytes) of the file with the given path.
     * @param absolutePath absolute path of the file
     * @return size in bytes, or zero (ambiguous!)
     */
    uint32_t fileSize(std::string absolutePath);

    /**
     * Opens the file denoted by the given path.
     * @param absolutePath the absolute path of the file to open
     * @return a file descriptor (handle) or zero (on error)
     */
    uint32_t openFile(std::string absolutePath);

    /**
     * Closes the file with the given handle.
     * If no such file is open, nothing happens.
     * Closing a file flushes all its unwritten contents and metadata to disk.
     * @param handle the file handle
     */
    void closeFile(uint32_t handle);

    /**
     * Flushes the file with the given handle to disk.
     * This forces pending metadata to be written to disk immediately,
     * preserving the data from loss in case of crashes etc.
     * @param handle the file handle
     */
    void flushFile(uint32_t handle);

    /**
     * Reads the requested number of bytes from the requested absolute position in the file denoted by fileHandle.
     * Places (copys) the data into the target buffer.
     * This method immediately returns with an error if the given byte range is invalid.
     * Byte ranges are only valid if every byte in them exists in the file.
     * @param fileHandle valid file descriptor
     * @param target buffer to copy the data into
     * @param pos absolute position within the file (bytes)
     * @param n number of bytes to copy
     * @return true, iff successful
     */
    bool read(uint32_t fileHandle, char* target, uint32_t pos, std::size_t n);

    /**
     * Writes the requested number of bytes from the requested absolute position in the file denoted by fileHandle.
     * Copys the data to disk.
     * Position must be within the file (data is overwritten, *not* inserted), or equal to the file size (data is appended).
     * If the file size is reached during writing, the file grows.
     * @param fileHandle valid file descriptor
     * @param source source to read data from
     * @param pos absolute position within file (bytes)
     * @param n number of bytes to copy
     * @return true, iff successful
     */
    bool write(uint32_t fileHandle, const char* source, uint32_t pos, std::size_t n);

    /**
     * Truncates the file denoted by the given handle to the given size.
     * After this operation returns true, the length of the file is exactly the given number of bytes.
     * All previous content that exceeds the given length is deleted.
     * This method will return an error when called with a length larger or equal to the file size.
     * @param fileHandle file descriptor of the file, must be open for this
     * @param size the new (smaller than current) size of the file
     * @return true, iff successful
     */
    bool truncate(uint32_t fileHandle, uint32_t size);

    virtual ~FS();
private:
    /**
     * Main access to underlying block device / partition.
     */
    STREAM &dev;

    /**
     * FS size in bytes.
     */
    uint64_t size_b;

    /**
     * Logic position in the log where to write the next block.
     */
    uint32_t write_ptr;

    /**
     * Next (currently free) block ID to use for new blocks.
     */
    uint32_t nextBlockID;

    /**
     * Position in bytes where the bmap starts.
     */
    uint64_t bmapStart_bptr;

    /**
     * Size of the bmap in bytes.
     */
    uint64_t bmapSize_b;

    /**
     * Position in bytes where the log starts.
     */
    uint64_t logStart_bptr;

    /**
     * Log logic size.
     */
    uint64_t logSize;

    /**
     * Number of currently used blocks.
     */
    uint32_t usedBlocks;

    /**
     * In-Memory block map.
     */
    uint32_t *bmap;

    /**
     * Used during fs mount, true iff the copy of the bmap on the disk is valid.
     * (last umount was successful)
     */
    bool dev_bmap_valid;

    /**
     * Callback for Directory objects, they use this to request the creation of new DirEntryLists.
     */
    IDirectoryEntryListCreator *dirEntryListCreator;

    /**
     * Callback for File objects, they use this to request the creation of new DataBlockLists.
     */
    IDataBlockListCreator *dataBlockListCreator;

    /**
     * Holds all open files. (File can only be opened once, for now).
     */
    std::unordered_map<uint32_t, std::unique_ptr<File>> openFiles;

    /**
     * Reads the header info from the block device.
     * Gets basic data and verifies this is actually a sdi4fs partition.
     * @return true, iff header ok.
     */
    bool readHeader();

    /**
     * Calculates parameters critical for fs operation,
     * included, but not limited to (TM):
     * - position & size of imap on disk
     * - start & size of log on disk
     */
    void calcLayout();

    /**
     * Loads the block map from disk in to main memory.
     */
    bool loadBMap();

    /**
     * Saves the block map back to disk.
     */
    void saveBMap();

    /**
     * Initializes the callbacks (creates anonymous implementations).
     */
    void initCallbacks();

    /**
     * Returns the latest location of a block in the log.
     * @param id the blockID
     * @return the address of the block, if known. zero if not in log/unknown/invalid
     */
    uint32_t lookupBlockAddress(uint32_t id);

    /**
     * Load the Directory (fs internal logic object) for the given primary DirectoryINode id.
     * @param id the id of the primary directoryINode
     * @return unique_ptr containing a Directory object on success, containing nullptr otherwise
     */
    std::unique_ptr<Directory> loadDirectory(uint32_t id);

    /**
     * Loads the DirectoryEntryList with the given id.
     * @param id the blockID
     * @return unique_ptr to the new list, to nullptr otherwise
     */
    std::unique_ptr<DirectoryEntryList> loadDirEntryList(uint32_t id);

    /**
     * Load the File (fs internal logic object) for the given primary FileINode id.
     * @param id the id of the primary fileINode
     * @return unique_ptr containing a File object on success, containing nullptr otherwise
     */
    std::unique_ptr<File> loadFile(uint32_t id);

    /**
     * Loads the DataBlockList with the given id.
     * @param id the blockID
     * @return unique_ptr to the new list, to nullptr otherwise
     */
    std::unique_ptr<DataBlockList> loadDataBlockList(uint32_t id);
    
    /**
     * Loads the DataBlock with the given id.
     * @param id the blockID
     * @return unique_ptr containing the DatBlock on success, nullptr otherwise
     */
    std::unique_ptr<DataBlock> loadDataBlock(uint32_t id);

    /**
     * (Re-)Generates the BMap from the contents of the Log.
     * Does two complete swipes of the Log: O(n)
     */
    void reconstructBMap();

    /**
     * Recursive, depth-first traversal function of the bmap-reconstruction (step 3)
     * @param bmapFilter the bmap filter to mark blocks as reachable
     * @param dir the dir to examine
     */
    void recursiveRecovery(bool *bmapFilter, Directory &dir);

    /**
     * Runs the garbage collection on-demand to find a allocable block in the log.
     * Returns a logic pointer to the next free position in the log.
     * Advances the write_ptr during search, but does *not* advance the pointer
     * when a result was found, so on success the returned value should be write_ptr.
     * Also does not change the number of used blocks.
     * @return logic pointer to free block in log, or zero iff full
     */
    uint32_t gc();

    /**
     * Returns a new (currently unused) blockID
     * @return new blockID
     */
    uint32_t getNextBlockID();

    /**
     * Saves the given Block to disk.
     * @param block block to save
     */
    void saveBlock(Block &block);

    /**
     * Frees all disk space used for the given block,
     * and removes it from the bmap.
     * @param id blockID
     */
    void freeBlock(uint32_t id);

    /**
     * Traverses the given path to find the parent directory of the given path.
     * The given object (last part of the path) does *not* need to exist for this.
     * @param absolutePath the absolute path
     * @return unique_ptr to the parent directory, or to nullptr if non-existent or invalid path
     */
    std::unique_ptr<Directory> searchParent(std::string absolutePath);

    /**
     * Peeks at the type field of the on-disk INode with the given id without fully loading it.
     * @param id blockID of an INode
     * @return inode type, undefined for non-INode ids
     */
    uint8_t peekINodeType(uint32_t id);

    /**
     * Converts the given file from inlined to non-inlined state.
     * @param file the file
     */
    void switchNonInline(File *file);

    /**
     * Adds a new DataBlock to the given file.
     * Also saves the currently cached content (if any) and sets the new Block as cached.
     * @param file the file
     * @param changedMetaBlocks map of changed meta blocks to save later, method will add changed blocks to this list, if not present
     * @return true, iff successful
     */
    bool addDataBlock(File *file, std::unordered_map<uint32_t, Block*> &changedMetaBlocks);

    /**
     * Removes n DataBlocks from the given file.
     * Always removes the last n DataBlocks (removes file content from the end).
     * Does not remove anything if the range is invalid
     * (n must be < #datablocks - 1, since the last DataBlock can never be removed).
     * @param file the file
     * @param n the number of DataBlocks to remove
     */
    void removeDataBlocks(File *file, std::size_t n);
};

} // SDI4FS

#endif	// SDI4FS_FS_H
