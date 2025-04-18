class ListNode:
    def __init__(self, val=0, next=None):
        self.val = val
        self.next = next

def mergeTwoLists(list1: ListNode, list2: ListNode) -> ListNode:
    # Create a dummy node to simplify the merging process
    dummy = ListNode(0)
    current = dummy
    
    # Traverse both lists and attach the smaller node each time
    while list1 and list2:
        if list1.val <= list2.val:
            current.next = list1
            list1 = list1.next
        else:
            current.next = list2
            list2 = list2.next
        current = current.next
    
    # Attach the remaining elements of list1 or list2
    current.next = list1 if list1 else list2
    
    return dummy.next

# Dummy Node: The dummy node is used to avoid edge cases where the merged list is empty initially. It provides a starting point to build the merged list.

# Comparison and Attachment: By iterating through both lists, we compare the current nodes and attach the smaller one to the merged list. This ensures the merged list remains sorted.

# Remaining Nodes Handling: After one list is exhausted, the remaining nodes of the other list (which are already sorted) are directly attached to the merged list.

# Efficiency: This approach operates in O(n + m) time complexity, where n and m are the lengths of the two lists, and uses O(1) extra space, making it optimal for the problem constraints.