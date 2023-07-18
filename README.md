Results
=======

| Time [us] | Type
| --------- | ----
| 344588 | std::allocator<int>
| 301716 | allocator::mallocator<int>
| 62831 | allocator::universal_allocator<int>
| 60555 | allocator::universal_allocator<int, 6, 4194304, allocator::mallocator>
| 57093 | allocator::block_allocator_adaptor<int>
| 56513 | allocator::block_allocator_adaptor<int, 4194304, allocator::mallocator>
