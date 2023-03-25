# cDedup

### FSC deduplication Usage
```
./init.sh
```
+ Backup a new workload into the system
```
./cDedup --task write --InputFile [backup workload] --ChunkingMethod fsc --Size [4 or 8 or 16] --MerkleTree [yes or no]
```
+ Restore a workload of from the system
```
./cDedup --task restore --RestorePath [path to restore] --RestoreRecipe [which version to restore(1 ~ no. of the last retained version)]
```

### CDC deduplication Usage
```
./init.sh
```
+ Backup a new workload into the system
```
./cDedup --task write --InputFile [backup workload] --ChunkingMethod cdc --Size [4 or 8 or 16] --Normal [0 or 1 or 2 or 3] --MerkleTree [yes or no]
```
+ Restore a workload of from the system
```
./cDedup --task restore --RestorePath [path to restore] --RestoreRecipe [which version to restore(1 ~ no. of the last retained version)]
```

### Full file deduplication Usage
```
./init.sh
```
+ Backup a new workload into the system
```
./cDedup --task write --InputFile [backup workload] --ChunkingMethod file
```
+ Restore a workload of from the system
```
./cDedup --task restore --RestorePath [path to restore] --RestoreId [which file id to restore] --ChunkingMethod file
```
