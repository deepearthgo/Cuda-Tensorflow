// FocalLoss.h
// Based on Officinal Tensorflow[github] Template : [https://github.com/tensorflow/tensorflow/tree/0c06d8d9aa88b0596b734c8feb6435d2cf359b7e/tensorflow/core]
// Based on FAIR Research Scientist He.Kaiming Group Paper [Focal Loss for Dense Object Detection] [https://arxiv.org/abs/1708.02002]

/* Copyright 2017
/* Author Deepearthgo
==============================================================================*/

#ifndef TENSORFLOW_KERNELS_FOCALLOSS_OP_H_
#define TENSORFLOW_KERNELS_FOCALLOSS_OP_H_
// Functor definition for FocalLossOp, must be compilable by nvcc.
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/tensor_types.h"

namespace tensorflow {
namespace functor {

// Functor used by FocalLossOp to do the computations.
template <typename Device, typename T>
struct FocalLoss {
  void operator()(const Device& d, typename TTypes<T, 2>::ConstMatrix input,
                  typename TTypes<T>::ConstVec labels,
                  float gamma,
                  float alpha,
                  typename TTypes<T>::Matrix output,
                  typename TTypes<T>::Matrix scratch1,
                  typename TTypes<T>::Matrix scratch2) {
    const int batch_size = input.dimension(0);
    const int n_class = input.dimension(1);
    //const int rest_size = input.size() / depth;

    Eigen::DSizes<int, 2> rest_by_batch(batch_size, n_class);
#if !defined(EIGEN_HAS_INDEX_LIST)
    Eigen::DSizes<int, 2> one_by_class(1, n_class);
    Eigen::DSizes<int, 2> batch_by_one(batch_size, 1);
    Eigen::DSizes<int, 1> along_class(n_class);
#else
    Eigen::IndexList<int, Eigen::type2index<1> > rest_by_batch;
    rest_by_batch.set(0, n_class);
    Eigen::IndexList<Eigen::type2index<1>, int> one_by_class;
    one_by_class.set(1, batch_size);
    Eigen::IndexList<int, Eigen::type2index<1> > batch_by_one;
    batch_by_one.set(0, batch_size);
    
#endif
    
    //scratch1||pro_
    scratch1(rest_by_batch).device(d) = 
              (input.reshape(rest_by_batch) - 
               input.reshape(rest_by_batch).
               maximum(along_class).
               eval().
               reshape(rest_by_batch).
               broadcast(one_by_class)).exp();
    
    //scratch2||pro_sum
    scratch2(rest_by_batch).device(d) =
              scratch1.reshape(rest_by_batch).
              reduce_sum(along_class).
              reshape(batch_by_one).
              broadcast(one_by_class);
    
    //output
    output.reshape(rest_by_batch).device(d) = 
      scratch1.div(scratch2).reshape(rest_by_batch);
    //pro_.reshape(rest_by_batch).device(d) =
    //  scratch1.div(scratch2).reshape(rest_by_batch);
    
    //_pt
    //_pt.reshape(batch_by_one).device(d) = scratch1(:,labels.reshape(batch_by_one));
   
  }
  
};

template <typename Device, typename T>
struct FocalLossGrad {
  void operator()(const Device& d, typename TTypes<T>::ConstMatrix input,
                  typename TTypes<T>::ConstVec labels,
                  typename TTypes<T, 2>::ConstMatrix out_backprop,
                  float gamma,
                  float alpha,
                  float variance_epsilon,
                  typename TTypes<T>::Matrix dx, 
                  typename TTypes<T>::Matrix scratch3
                  typename TTypes<T>::Matrix scratch4
                  typename TTypes<T>::Matrix scratch5,
                  typename TTypes<T>::Vec scratch6,
                  typename TTypes<T>::Vec scratch7) {
    const int batch_size = pro_.dimension(0);
    const int n_class = pro_.dimension(1);
    
    typedef typename TTypes<T>::ConstVec::Index Index;

    Eigen::DSizes<int, 2> rest_by_batch(batch_size, n_class);
#if !defined(EIGEN_HAS_INDEX_LIST)
    Eigen::DSizes<int, 2> one_by_class(1, n_class);
    Eigen::DSizes<int, 2> batch_by_one(batch_size, 1);
    Eigen::DSizes<int, 1> along_class(1);
    
#else
    Eigen::IndexList<int, Eigen::type2index<1> > rest_by_batch;
    rest_by_batch.set(0, n_class);
    Eigen::IndexList<Eigen::type2index<1>, int> one_by_class;
    one_by_class.set(1, batch_size);
    Eigen::IndexList<int, Eigen::type2index<1> > batch_by_one;
    batch_by_one.set(0, batch_size);
    
#endif
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    //scratch3||pro_1
    scratch3(rest_by_batch).device(d) = 
              (input.reshape(rest_by_batch) - 
               input.reshape(rest_by_batch).
               maximum(along_class).
               eval().
               reshape(rest_by_batch).
               broadcast(one_by_class)).exp();
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    //scratch4||pro_sum
    scratch4(rest_by_batch).device(d) =
              scratch3.reshape(rest_by_batch).
              reduce_sum(along_class).
              reshape(batch_by_one).
              broadcast(one_by_class);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    //scratch5||pro_
    scratch5.reshape(rest_by_batch).device(d) =
       scratch3.div(scratch4).reshape(rest_by_batch);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    //scratch6||_pt
    scratch6.reshape(batch_by_one).device(d) = scratch5(:,labels.reshape(batch_by_one));
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    // scratch7 || _pt + epsilon
    scratch7.device(d) = (scratch6.reshape(batch_by_one) + variance_epsilon).eval().reshape(batch_by_one);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++// 
    // i!=j
    dx.device(d) = (static_cast<T>(1.0) - scratch7).pow(gamma-static_cast<T>(1.0)).
                    mul(alpha).reshape(batch_by_one).broadcast(one_by_class).
                    mul((scratch7).log().reshape(batch_by_one).broadcast(one_by_class).
                    mul((scratch5).mul(scratch7.boardcast(one_by_class)).mul(gamma*-static_cast<T>(1.0)))+
                    scratch5.mul((1-scratch7).reshape(batch_by_one).broadcast(one_by_batch))*out_backprop;    
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++//
    // i==j
    dx.device(d)(:,labels.reshape(batch_by_one)) = (static_cast<T>(1.0) - scratch7).pow(gamma).reshape(batch_by_one).mul(alpha)
                       .mul(scratch7.mul(scratch7.log()).mul(gamma)+scratch7-static_cast<T>(1.0))
                       .mul(static_cast<T>(1.0))*out_backprop(:,labels.reshape(batch_by_one)); 
                        
  }

};

}  // namespace functor
}  // namespace tensorflow

#endif  //TENSORFLOW_KERNELS_FOCALLOSS_OP_H_
