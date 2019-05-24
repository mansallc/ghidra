/* ###
 * IP: GHIDRA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/// \file rangeutil.hh
/// \brief Documentation for the CircleRange class
#ifndef __RANGEUTIL__
#define __RANGEUTIL__

#include "op.hh"

/// \brief A class for manipulating integer value ranges.
///
/// The idea is to have a representation of common sets of
/// values that a varnode might take on in analysis so that
/// the representation can be manipulated symbolically to
/// some extent.  The representation is a circular range
/// (determined by a half-open interval [left,right)), over
/// the integers mod 2^n,  where mask = 2^n-1.
/// The range can support a step, if some of the
/// least significant bits of the mask are set to zero.
///
/// The class then can
///   - Generate ranges based on a pcode condition:
///      -    x < 2      =>   left=0  right=2  mask=sizeof(x)
///      -    5 >= x     =>   left=5  right=0  mask=sizeof(x)
///
///   - Intersect and union ranges, if the result is another range
///   - Pull-back a range through a transformation operation
///   - Iterate
///
///   \code
///     val = range.getMin();
///     do {
///     } while(range.getNext(val));
///   \endcode
class CircleRange {
  uintb left;			///< Left boundary of the open range [left,right)
  uintb right;			///< Right boundary of the open range [left,right)
  uintb mask;			///< Bit mask defining the size (modulus) and stop of the range
  bool isempty;			///< \b true if set is empty
  int4 step;			///< Explicit step size
  static const char arrange[];	///< Map from raw overlaps to normalized overlap code
  void normalize(void);		///< Normalize the representation of full sets
  void complement(void);	///< Set \b this to the complement of itself
  bool convertToBoolean(void);	///< Convert \b this to boolean.
  static bool newStride(uintb mask,int4 step,int4 oldStep,uint4 rem,uintb &myleft,uintb &myright);
  static bool newDomain(uintb newMask,int4 newStep,uintb &myleft,uintb &myright);
  static char encodeRangeOverlaps(uintb op1left,uintb op1right,uintb op2left,uintb op2right);	///< Calculate overlap code
public:
  CircleRange(void) { isempty=true; }		///< Construct an empty range
  CircleRange(uintb lft,uintb rgt,int4 size,int4 stp);	///< Construct given specific boundaries.
  CircleRange(bool val);			///< Construct a boolean range
  CircleRange(uintb val,int4 size);		///< Construct range with single value
  void setRange(uintb lft,uintb rgt,int4 size,int4 step);	///< Set directly to a specific range
  void setRange(uintb val,int4 size);		///< Set range with a single value
  void setFull(int4 size);			///< Set a completely full range
  bool isEmpty(void) const { return isempty; }	///< Return \b true if \b this range is empty
  bool isFull(void) const { return ((!isempty) && (step == 1) && (left == right)); }	///< Return \b true if \b this contains all possible values
  bool isSingle(void) const { return (!isempty) && (right == ((left + step)& mask)); }	///< Return \b true if \b this contains single value
  uintb getMin(void) const { return left; }	///< Get the left boundary of the range
  uintb getMax(void) const { return (right-step)&mask; }	///< Get the right-most integer contained in the range
  uintb getEnd(void) const { return right; }	///< Get the right boundary of the range
  uintb getMask(void) const { return mask; }	///< Get the mask
  uintb getSize(void) const;			///< Get the size of this range
  int4 getStep(void) const { return step; }	///< Get the step for \b this range
  int4 getMaxInfo(void) const;			///< Get maximum information content of range
  bool operator==(const CircleRange &op2) const;	///< Equals operator
  bool getNext(uintb &val) const { val = (val+step)&mask; return (val!=right); }	///< Advance an integer within the range
  bool contains(const CircleRange &op2) const;	///< Check containment of another range in \b this.
  bool contains(uintb val) const;		///< Check containment of a specific integer.
  int4 intersect(const CircleRange &op2);	///< Intersect \b this with another range
  bool setNZMask(uintb nzmask,int4 size);	///< Set the range based on a putative mask.
  int4 circleUnion(const CircleRange &op2);	///< Union two ranges.
  bool minimalContainer(const CircleRange &op2,int4 maxStep);	///< Construct minimal range that contains both \b this and another range
  int4 invert(void);				///< Convert to complementary range
  void setStride(int4 newStep,uintb rem);	///< Set a new step on \b this range.
  bool pullBackUnary(OpCode opc,int4 inSize,int4 outSize);	///< Pull-back \b this through the given unary operator
  bool pullBackBinary(OpCode opc,uintb val,int4 slot,int4 inSize,int4 outSize);	///< Pull-back \b this thru binary operator
  Varnode *pullBack(PcodeOp *op,Varnode **constMarkup,bool usenzmask);	///< Pull-back \b this range through given PcodeOp.
  bool pushForwardUnary(OpCode opc,const CircleRange &in1,int4 inSize,int4 outSize);	///< Push-forward thru given unary operator
  bool pushForwardBinary(OpCode opc,const CircleRange &in1,const CircleRange &in2,int4 inSize,int4 outSize,int4 maxStep);
  void widen(const CircleRange &op2,bool leftIsStable);	///< Widen the unstable bound to match containing range
  int4 translate2Op(OpCode &opc,uintb &c,int4 &cslot) const;	///< Translate range to a comparison op
  void printRaw(ostream &s) const;		///< Write a text representation of \b this to stream
};

class Partition;		// Forward declaration

/// \brief A range of values attached to a Varnode within a data-flow subsystem
///
/// This class acts as both the set of values for the Varnode and as a node in a
/// sub-graph overlaying the full data-flow of the function containing the Varnode.
/// The values are stored in the CircleRange field and can be interpreted either as
/// absolute values (if \b typeCode is 0) or as values relative to a stack pointer
/// or some other register (if \b typeCode is non-zero).
class ValueSet {
public:
  class Equation {
    friend class ValueSet;
    int4 slot;
    CircleRange range;
  public:
    Equation(int4 s,const CircleRange &rng) { slot=s; range = rng; }	///< Constructor
  };
private:
  friend class ValueSetSolver;
  int4 typeCode;	///< 0=pure constant 1=stack relative
  Varnode *vn;		///< Varnode whose set this represents
  OpCode opCode;	///< Op-code defining Varnode
  int4 numParams;	///< Number of input parameters to defining operation
  CircleRange range;	///< Range of values or offsets in this set
  int4 count;		///< Depth first numbering / widening count
  vector<Equation> equations;	///< Any equations associated with this value set
  Partition *partHead;	///< If Varnode is a component head, pointer to corresponding Partition
  ValueSet *next;	///< Next ValueSet to iterate
  void setVarnode(Varnode *v,int4 tCode);	///< Attach \b this to given Varnode and set initial values
  void addEquation(int4 slot,const CircleRange &constraint);	///< Insert an equation restricting \b this value set
  void addLandmark(const CircleRange &constraint) { addEquation(numParams,constraint); }	///< Add a widening landmark
  void doWidening(const CircleRange &newRange);	///< Widen the value set so fixed point is reached sooner
  void looped(void);	///< Mark that iteration has looped back to \b this
  bool iterate(void);				///< Regenerate \b this value set from operator inputs
public:
  int4 getTypeCode(void) const { return typeCode; }	///< Return '0' for normal constant, '1' for spacebase relative
  Varnode *getVarnode(void) const { return vn; }	///< Get the Varnode attached to \b this ValueSet
  const CircleRange &getRange(void) const { return range; }	///< Get the actual range of values
  void printRaw(ostream &s) const;		///< Write a text description of \b to the given stream
};

/// \brief A range of nodes (within the weak topological ordering) that are iterated together
class Partition {
  friend class ValueSetSolver;
  ValueSet *startNode;		///< Starting node of component
  ValueSet *stopNode;		///< Ending node of component
  bool isDirty;			///< Set to \b true if a node in \b this component has changed this iteration
public:
  Partition(void) {
    startNode = (ValueSet *)0; stopNode = (ValueSet *)0; isDirty = false;
  }				///< Construct empty partition
};

/// \brief Class the determines a ValueSet for each Varnode in a data-flow system
///
/// This class uses \e value \e set \e analysis to calculate (an overestimation of)
/// the range of values that can reach each Varnode.  The system is formed by providing
/// a set of Varnodes for which the range is desired (the sinks) via establishValueSets().
/// This creates a system of Varnodes (within the single function) that can flow to the sinks.
/// Running the method solve() does the analysis, and the caller can examine the results
/// by examining the ValueSet attached to any of the Varnodes in the system (via Varnode::getValueSet()).
class ValueSetSolver {
  /// \brief An iterator over out-bound edges for a single ValueSet node in a data-flow system
  ///
  /// This is a helper class for walking a collection of ValueSets as a graph.
  /// Mostly the graph mirrors the data-flow of the Varnodes underlying the ValueSets, but
  /// there is support for a simulated root node. This class acts as an iterator over the outgoing
  /// edges of a particular ValueSet in the graph.
  class ValueSetEdge {
    const vector<ValueSet *> *rootEdges;		///< The list of nodes attached to the simulated root node (or NULL)
    int4 rootPos;					///< The iterator position for the simulated root node
    Varnode *vn;					///< The Varnode attached to a normal ValueSet node (or NULL)
    list<PcodeOp *>::const_iterator iter;		///< The iterator position for a normal ValueSet node
  public:
    ValueSetEdge(ValueSet *node,const vector<ValueSet *> &roots);
    ValueSet *getNext(void);
  };

  list<ValueSet> valueNodes;		///< Storage for all the current value sets
  Partition orderPartition;		///< Value sets in iteration order
  list<Partition> recordStorage;	///< Storage for the Partitions establishing components
  vector<ValueSet *> rootNodes;		///< Values treated as inputs
  vector<ValueSet *> nodeStack;		///< Stack used to generate the topological ordering
  int4 depthFirstIndex;			///< (Global) depth first numbering for topological ordering
  int4 numIterations;			///< Count of individual ValueSet iterations
  int4 maxIterations;			///< Maximum number of iterations before forcing termination
  void newValueSet(Varnode *vn,int4 tCode);		///< Allocate storage for a new ValueSet
  static void partitionPrepend(ValueSet *vertex,Partition &part);	///< Prepend a vertex to a partition
  static void partitionPrepend(const Partition &head,Partition &part);	///< Prepend full Partition to given Partition
  void partitionSurround(Partition &part);				///< Create a full partition component
  void component(ValueSet *vertex,Partition &part);		///< Generate a partition component given its head
  int4 visit(ValueSet *vertex,Partition &part);			///< Recursively walk the data-flow graph finding partitions
  void establishTopologicalOrder(void);				///< Find the optimal order for iterating through the ValueSets
  void applyConstraints(Varnode *vn,const CircleRange &range,FlowBlock *splitPoint);
  void constraintsFromPath(Varnode *vn,PcodeOp *cbranch);	///< Generate constraints given a branch and matching Varnode
  void constraintsFromCBranch(PcodeOp *cbranch);		///< Generate constraints arising from the given branch
  void generateConstraints(vector<Varnode *> &worklist);	///< Generate constraints given a system of Varnodes
public:
  void establishValueSets(const vector<Varnode *> &sinks,Varnode *stackReg);	///< Build value sets for a data-flow system
  int4 getNumIterations(void) const { return numIterations; }	///< Get the current number of iterations
  void solve(int4 max);				///< Iterate the ValueSet system until it stabilizes
  list<ValueSet>::const_iterator beginValueSets(void) { return valueNodes.begin(); }	///< Start of all ValueSets in the system
  list<ValueSet>::const_iterator endValueSets(void) { return valueNodes.end(); }	///< End of all ValueSets in the system
};

/// \param op2 is the range to compare \b this to
/// \return \b true if the two ranges are equal
inline bool CircleRange::operator==(const CircleRange &op2) const

{
  if (isempty != op2.isempty) return false;
  if (isempty) return true;
  return (left == op2.left) && (right == op2.right) && (mask == op2.mask) && (step == op2.step);
}

/// If two ranges are labeled [l , r) and  [op2.l, op2.r), the
/// overlap of the ranges can be characterized by listing the four boundary
/// values  in order, as the circle is traversed in a clock-wise direction.  This characterization can be
/// further normalized by starting the list at op2.l, unless op2.l is contained in the range [l, r).
/// In which case, the list should start with l.  You get the following 6 categories
///    - a  = (l r op2.l op2.r)
///    - b  = (l op2.l r op2.r)
///    - c  = (l op2.l op2.r r)
///    - d  = (op2.l l r op2.r)
///    - e  = (op2.l l op2.r r)
///    - f  = (op2.l op2.r l r)
///    - g  = (l op2.r op2.l r)
///
/// Given 2 ranges, this method calculates the category code for the overlap.
/// \param op1left is left boundary of the first range
/// \param op1right is the right boundary of the first range
/// \param op2left is the left boundary of the second range
/// \param op2right is the right boundary of the second range
/// \return the character code of the normalized overlap category
inline char CircleRange::encodeRangeOverlaps(uintb op1left, uintb op1right, uintb op2left, uintb op2right)

{
  int4 val = (op1left <= op1right) ? 0x20 : 0;
  val |= (op1left <= op2left) ? 0x10 : 0;
  val |= (op1left <= op2right) ? 0x8 : 0;
  val |= (op1right <= op2left) ? 4 : 0;
  val |= (op1right <= op2right) ? 2 : 0;
  val |= (op2left <= op2right) ? 1 : 0;
  return arrange[val];
}

/// \param vertex is the node that will be prepended
/// \param part is the Partition being modified
inline void ValueSetSolver::partitionPrepend(ValueSet *vertex,Partition &part)

{
  vertex->next = part.startNode;	// Attach new vertex to beginning of list
  part.startNode = vertex;		// Change the first value set to be the new vertex
  if (part.stopNode == (ValueSet *)0)
    part.stopNode = vertex;
}

/// \param head is the partition to be prepended
/// \param part is the given partition being modified (prepended to)
inline void ValueSetSolver::partitionPrepend(const Partition &head,Partition &part)

{
  head.stopNode->next = part.startNode;
  part.startNode = head.startNode;
  if (part.stopNode == (ValueSet *)0)
    part.stopNode = head.stopNode;
}

#endif
