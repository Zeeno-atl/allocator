Results
=======

# Debian

| Time [us] | Speed | Type
| --------- | ----- | ----
| 344588 | 1 | std::allocator<int>
| 301716 | 1.14 | allocator::mallocator<int>
| 62831 | 5.48 | allocator::universal_allocator<int>
| 60555 | 5.69 | allocator::universal_allocator<int, 6, 4194304, allocator::mallocator>
| 57093 | 6.04 | allocator::block_allocator_adaptor<int>
| 56513 | 6.1 | allocator::block_allocator_adaptor<int, 4194304, allocator::mallocator>

# Windows
| Time [us] | Speed | Type
| --------- | ----- | ----
766740 | 1 | class std::allocator<int>
738434 | 1.04 | struct allocator::mallocator<int>
89338 | 8.58 | struct allocator::universal_allocator<int,6,4194304,class std::allocator>
98753 | 7.76 | struct allocator::universal_allocator<int,6,4194304,struct allocator::mallocator>
87307 | 8.78 | struct allocator::block_allocator_adaptor<int,4194304,class std::allocator>
91747 | 8.36 | struct allocator::block_allocator_adaptor<int,4194304,struct allocator::mallocator>
