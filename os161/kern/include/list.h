#ifndef LIST_H
#define LIST_H

#define REMOVEHEAD_HT(head, tail, node) \
{\
    if ((tail) == (head)) { \
        (node) = (head);\
        (head) = NULL;\
        (tail) = NULL;\
    } else {\
        (node) = (head);\
        (head) = (head)->next;\
        assert((head) != NULL);\
        assert((tail) != NULL);\
    }\
    if (node) (node)->next = NULL;\
}

#define ADDTOTAIL_HT(head, tail, node) \
{\
    assert(node != NULL);\
    assert(node->next == NULL);\
    if ((tail) == NULL) {\
        assert((head) == NULL);\
        (tail) = (node); \
        (head) = (node);\
    } else {\
        assert((head) != NULL);\
        (tail)->next = (node);\
        (tail) = (node);\
    }\
}

#define REMOVEHEAD_H(head, node) \
{\
    if (head) {\
        node = head;\
        head = head->next;\
        if (node) node->next = NULL;\
    }\
}

#define ADDTOHEAD_H(head, node)\
{\
    assert(node->next == NULL);\
    node->next = head;\
    head = node;\
}

#define ISEMPTY(head) ((head) == NULL)
#endif
