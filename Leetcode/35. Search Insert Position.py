class Solution:
    def searchInsert(self, nums: List[int], target: int) -> int:
        left, right = 0, len(nums) - 1
        while left <= right:
            mid = (left + right) // 2
            if nums[mid] == target:
                return mid
            elif nums[mid] < target:
                left = mid + 1
            else:
                right = mid - 1
        return left

# Binary Search: This algorithm efficiently reduces the search space by half in each iteration, leading to O(log n) time complexity.

# Insertion Logic: The loop terminates when left exceeds right, indicating the target is not found. The left pointer at this stage points to the position where the target should be inserted to maintain the sorted order.

# Edge Cases: Handles insertion at the beginning, middle, or end of the array seamlessly by the natural termination of the binary search loop.

